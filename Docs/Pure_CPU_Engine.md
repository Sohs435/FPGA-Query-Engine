# C++ Query Engine — CPU Stage Report

**Status:** Functional in-memory query engine, with integration tests and desktop CPU benchmarks. This report covers the C++ implementation only; the benchmark figures do not include FPGA hardware.

## What was implemented

The engine stores tables in a **columnar layout**: each named, typed column owns a contiguous vector of `Int32` or `Int64` values. `Schema` records column names and types, and `Table` checks that its columns have compatible types and equal row counts. A CSV loader builds tables from input data. A small table registry resolves the name in `FROM`.

`QueryEngine::execute(sql)` connects the stages:

1. **Tokenize:** Turn SQL text into identifiers, integer literals, keywords, operators and punctuation. Keyword matching is case-insensitive.
2. **Parse:** Build a `ParsedQuery` containing `SELECT` items, a table name, an optional `WHERE` predicate and optional `GROUP BY` names. Scalar and predicate syntax becomes expression trees built with `std::variant` and `std::unique_ptr`.
3. **Bind:** Resolve column names against the table schema once. The binder converts references such as `price` into a column index and type, binds selected expressions and `WHERE`, converts `GROUP BY` names to indices, and checks aggregate/grouping rules. Unknown columns and invalid combinations fail before row processing.
4. **Execute:** Evaluate `WHERE` into a selection mask, then return projected rows, a single aggregate row, or grouped aggregate rows as a typed `QueryResult`. A hash map tracks groups by their integer key values.

The supported query path covers projections, aliases, arithmetic (`+`, `-`, `*`, `/`), comparisons, `AND`/`OR`/`NOT`, integer `IN` lists, `BETWEEN`, `COUNT`, `SUM`, `MIN`, `MAX`, and multiple `GROUP BY` columns. The evaluator checks integer arithmetic and aggregate addition for overflow and rejects division by zero. `COUNT` and `SUM` results use `Int64`; ordinary projection and `MIN`/`MAX` retain the expression's type. Output aliases name result columns.

## Parsing and precedence

The parser uses separate recursive descent functions for arithmetic and Boolean expressions. From **highest to lowest precedence**:

| Level | Syntax | Example interpretation |
| --- | --- | --- |
| Grouping and values | `(…)`, column names, integers | Parentheses determine the contained expression first. |
| Unary arithmetic | `+x`, `-x` | `-price * 2` groups as `(-price) * 2`. |
| Multiplicative | `*`, `/` | `price + quantity * 2` multiplies first. |
| Additive | `+`, `-` | `price + quantity * 2` adds last. |
| Predicates | Comparisons, `IN`, `BETWEEN` | `price * quantity > 1000` compares the product. |
| Boolean `NOT` | `NOT predicate` | `NOT price > 1000` negates the comparison. |
| Boolean `AND` | `left AND right` | Evaluated before `OR`. |
| Boolean `OR` | `left OR right` | Lowest predicate precedence. |

For example, `price + quantity * 2 > 1000 AND instrument IN (1, 2)` becomes an `AND` node whose left child compares `price + (quantity * 2)` with `1000`, and whose right child checks membership in `(1, 2)`. Parentheses can override either arithmetic or Boolean grouping. Within `BETWEEN low AND high`, that `AND` separates the two bounds; it is consumed as part of `BETWEEN`.

This grammar is an intentionally limited SQL subset. It does not yet provide joins, `ORDER BY`, `LIMIT`, string values or SQL `NULL` semantics.

## Functional verification

The reported integration run passed **11 checks** on a 30-row `trades` CSV:

- Projection checked result shape, values and the `trade_value` alias for `price * quantity`.
- Aggregation checked `COUNT(*) = 20`, `SUM(price) = 31,450`, `MIN(price) = 850` and `MAX(price) = 2,150` for `quantity > 500`, together with result shape and aliases.
- `GROUP BY instrument` checked five groups and their counts and price sums, matching groups by instrument value rather than assuming a result order.
- Invalid queries checked rejection of an unknown table, an unknown column, and a selected non-grouped column mixed with an aggregate.

Earlier parser/filter runs also exercised nested predicates, arithmetic comparisons and malformed queries. These checks establish the implemented paths on the small test dataset; they do not constitute exhaustive SQL conformance testing.

## CPU benchmark results

Synthetic tables contained five `Int32` columns and one `Int64` column, approximately **28 bytes per row**. An optimized desktop build timed repeated queries after warm-ups. “Execution only” used a previously parsed and bound query; “end to end” also included tokenization, parsing, lookup and binding. The benchmark consumed results so the operations could not simply be discarded. The values below are **median execution-only times**, with five timed iterations at each listed size.

| Workload | 10 million rows | 50 million rows |
| --- | ---: | ---: |
| Column scan + `SUM(price)` | 35.276 ms | 161.244 ms |
| Filter 50% + `COUNT(*)` | 60.677 ms | 323.718 ms |
| `SUM(price + quantity)` | 97.457 ms | 476.443 ms |
| `SUM(price * quantity)` | 107.398 ms | 521.121 ms |
| Filtered multiplication + `SUM` | 113.551 ms | 525.472 ms |
| Projection at 50% selectivity | 124.258 ms | 637.645 ms |
| `GROUP BY` 1,000 keys | 439.907 ms | 2,233.384 ms |
| `GROUP BY` 100,000 keys | 543.914 ms | 2,740.949 ms |

For context, a direct C++ loop over the two input vectors completed multiplication plus sum in **3.273 ms** at 10 million rows and **16.504 ms** at 50 million. At 10 million rows, generic query-engine multiplication plus sum took about **107 ms**; addition plus sum took about **97 ms**. The gap from the direct loop reflects costs of the general query path, including expression evaluation and result handling. It does **not** establish that integer multiplication itself is the dominant bottleneck.

## Assessment and next measurement

The CPU stage can accept SQL and produce typed projection, aggregate and grouped results. Its clearest measured cost is general query execution, especially grouping and expression evaluation, relative to simple loops. These figures come from a desktop CPU; the 50-million-row table is approximately **1.3 GiB** and is beyond the PYNQ-Z2's stated 512 MB DDR capacity. The measurements should therefore serve as a functional and performance baseline for the desktop implementation, not as a predicted FPGA speedup.

Before selecting hardware operators, measure the same optimized C++ queries on the PYNQ's ARM CPU and measure sustained PS DDR-to-PL transfer time. Those two same-board figures define the actual break-even point for acceleration.
