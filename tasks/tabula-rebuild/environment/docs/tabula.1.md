% TABULA(1) tabula 2.3.1 | User Commands

# NAME

tabula - filter, sort, group and print CSV and TSV tables

# SYNOPSIS

`tabula [OPTION]... [FILE]`

# DESCRIPTION

tabula reads one delimited table from FILE, or from standard input when FILE
is absent or is `-`, applies the requested processing and writes the result
to standard output. By default the first record is a header that names the
columns, and the output is CSV with the same header.

Options may appear before or after FILE. Long options may be abbreviated to
any unambiguous prefix and take their value either as `--opt=VALUE` or as the
next argument. Short options may be grouped (`-tu`) and take their value
either attached (`-n5`) or as the next argument. `--` ends option processing.

# INPUT

Fields are separated by the delimiter (`,` by default, TAB when FILE ends in
`.tsv`). A field may be enclosed in double quotes, in which case it may
contain the delimiter, newlines and doubled quotes (`""`). Records end with
LF or CRLF. A UTF-8 byte order mark at the start of the input is ignored.

`-d`, `--delimiter=CHAR`
:   Input delimiter: a single character, or `tab`.

`-H`, `--no-header`
:   The input has no header row. Columns are named `c1`, `c2`, ... and no
    header is printed.

`--trim`
:   Remove leading and trailing blanks from every field, header included.

`--strict`
:   Fail when a data row has a different number of fields than the header.
    Without it, short rows are padded and long rows are cut.

# SELECTING COLUMNS

A column LIST is a comma separated list of items. Each item is a column name,
a 1-based column number, or a range of numbers such as `2-4`.

`-s`, `--select=LIST`
:   Output only the listed columns, in list order.

`-x`, `--exclude=LIST`
:   Output all columns except the listed ones.

# FILTERING

`-w`, `--where=EXPR`
:   Keep rows for which EXPR is true. May be given more than once; a row must
    satisfy every expression.

An expression compares operands with `=` (or `==`), `!=` (or `<>`), `<`,
`<=`, `>`, `>=`, `~` (contains), `^=` (starts with) and `$=` (ends with), and
combines comparisons with `and`, `or`, `not` (also `&&`, `||`, `!`) and
parentheses. Operands are column names, `$N` for the N-th column, a column
name in backquotes (for names that are not plain words), numbers, and strings
in single or double quotes. Values that are both numbers are compared as
numbers; otherwise they are compared as text.

Example: `tabula -w "qty > 5 and region = 'north'" sales.csv`

# SORTING

`-S`, `--sort=KEYS`
:   Sort the rows. KEYS is a comma separated list of `COL[:FLAGS]` where COL
    is a column name or number and FLAGS is any of `n` (numeric), `N`
    (natural, so that `file2` sorts before `file10`), `r` (reverse) and `i`
    (ignore case). Rows that compare equal keep their input order.

# GROUPING

`-g`, `--group=LIST`
:   Produce one output row per distinct combination of the listed columns.

`-a`, `--agg=SPECS`
:   Aggregate columns. SPECS is a comma separated list of `[NAME=]FUNC(COL)`
    where FUNC is one of `count`, `sum`, `avg`, `min`, `max`, `first`,
    `last` and `distinct`, and `count(*)` counts rows. Without NAME the
    output column is named after the function and the column. Without
    `--group` the whole table forms one group.

Grouped output contains the group columns followed by the aggregates. Sort
keys given together with grouping refer to the grouped output.

# OUTPUT

`-u`, `--unique`
:   Drop output rows that repeat an earlier output row.

`-k`, `--skip=N`
:   Skip the first N output rows.

`-n`, `--limit=N`
:   Print at most N rows.

`-C`, `--count`
:   Print only the number of rows that would have been printed.

`-c`, `--columns`
:   Print the number and name of every input column, one per line.

`-D`, `--out-delimiter=CHAR`
:   Output delimiter. Defaults to the input delimiter.

`-N`, `--no-header-out`
:   Do not print the header row.

`-q`, `--quote=MODE`
:   `minimal` (the default) quotes fields only when needed, `all` quotes
    every field, `nonnumeric` quotes fields that are not numbers, and `none`
    never quotes and reports an error if a field cannot be written safely.

`-e`, `--empty=STR`
:   Print STR in place of empty fields.

`-p`, `--precision=N`
:   Print numeric fields with exactly N digits after the decimal point.

`-r`, `--round=MODE`
:   Rounding used by `--precision`: `half-even`, `half-up`, `down` (toward
    zero) or `up` (away from zero).

`-T`, `--thousands`
:   Separate groups of three integer digits with commas.

`-t`, `--pretty`
:   Print an aligned, human readable table. Column widths account for wide
    and combining characters.

`-m`, `--max-width=N`
:   With `--pretty`, shorten cells that are wider than N columns.

`-h`, `--help`
:   Show a summary of the options.

`-V`, `--version`
:   Show the version.

# EXAMPLES

    $ tabula -s name,age people.csv
    name,age
    Ana,34
    Bo,29

    $ tabula -S age:n -t people.csv
    name | age
    -----+----
    Bo   |  29
    Ana  |  34

    $ tabula -g region -a "total=sum(qty)" sales.csv
    region,total
    north,24
    south,6

# EXIT STATUS

0 success; 1 invalid command line; 2 invalid expression, sort key or
aggregate specification; 3 reference to a column that does not exist;
4 output that cannot be written in the requested quoting mode; 5 malformed
or empty input; 6 a file that cannot be opened or read.

Diagnostics are written to standard error, prefixed with `tabula: `.
