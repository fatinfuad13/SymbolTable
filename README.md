# Symbol Table
**Course:** CSE310 — Compiler Sessional | **Session:** January 2026 | **Student ID:** 2205101

A symbol table implementation for a compiler, using a stack of hash tables to handle scoping. Each scope is a separate hash table (Scope Table), symbols are hashed using SDBM, and collisions are resolved via chaining.

## File Structure

```
SymbolTable/
├── 2205101_symbol_table.cpp
├── 2205101_build.sh
└── sample_input.txt
```

## Building and Running

```bash
chmod +x 2205101_build.sh
./2205101_build.sh <input_file> <output_file>
```

## Input Format

First line is the number of buckets. Each following line is a command:

| Command | Description |
|---------|-------------|
| `I <name> <type>` | Insert symbol into current scope |
| `L <name>` | Look up symbol across all scopes |
| `D <name>` | Delete symbol from current scope |
| `S` | Enter a new scope |
| `E` | Exit current scope |
| `P C` / `P A` | Print current / all scope tables |
| `Q` | Quit |

FUNCTION, STRUCT, and UNION types take extra tokens — see sample input for format.
