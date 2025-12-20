# JSON Parser and Validator

This project provides a lightweight, header-only JSON library in C++ (`json.hpp`) for creating, parsing, validating, and manipulating JSON documents. It also includes three example programs:

* **`validate-json.cpp`** — validates JSON files and benchmarks parsing time
* **`create-json.cpp`** — demonstrates programmatic JSON construction
* **`manipulate-json.cpp`** — demonstrates safe navigation and modification of JSON data

---

## Additional Information

* **Inspiration**: [Coding Challenges JSON Parser](https://codingchallenges.fyi/challenges/challenge-json-parser)
* **Data Structure Credits**:
  [Stack Overflow: Data types for representing JSON in C](https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c)

---

## Design Decisions

1. Mimic (imperfectly) Python’s `json` module with `loads` and `dumps`.
2. Prefer throwing descriptive errors over silently returning invalid values.
3. Use `std::vector` for both arrays and objects to preserve insertion order.
4. Support arbitrary nesting depth.
5. Expose a high-level, safe **view abstraction (`JSONHandle`)** over raw node pointers.

---

## Requirements

* **Compiler**: C++23-capable compiler (tested with `g++`)
* **CMake**: for building and running tests

---

## Project Structure

* **json.hpp** — Header-only JSON library
* **validate-json.cpp** — Validates JSON files and benchmarks parsing time
* **create-json.cpp** — Demonstrates JSON creation
* **manipulate-json.cpp** — Demonstrates JSON navigation and modification
* **test/** — Test JSON files (`passXX.json`, `failXX.json`, `benchXX.json`)
  * Includes additional tests from
    [JSONTestSuite](https://github.com/nst/JSONTestSuite/tree/master/test_parsing) under `test/extras/`

---

## Building the Project

```bash
cmake -S . -B build
cmake --build build -j4
```

This produces the following executables in `build/`:

* `validate-json`
* `create-json`
* `manipulate-json`

---

## Running the JSON Validator

Tests are registered with CTest.

```bash
cd build
ctest
```

To see full output from the validator:

```bash
ctest --verbose --test-dir build
```

---

## Creating Sample JSON Documents

Example from `create-json.cpp`:

```cpp
JSON::JSONNodePtr one  = JSON::helper::createNode(1);
JSON::JSONNodePtr two  = JSON::helper::createNode(2);
JSON::JSONNodePtr arr  = JSON::helper::createArray("array", {one, two});

JSON::JSONNodePtr num  = JSON::helper::createNode("number", 123);
JSON::JSONNodePtr str  = JSON::helper::createNode("string", "Hello World");

JSON::JSONNodePtr root = JSON::helper::createObject({arr, num, str});

std::cout << JSON::Parser::dumps(root) << "\n";
```

Output:

```json
{"array":[1,2],"number":123,"string":"Hello World"}
```

---

## Core Types

### JSON Node Hierarchy

* **`JSONNode`** — Base class for all nodes
* **`JSONValueNode`** — Simple values (`string`, `number`, `bool`, `null`)
* **`JSONArrayNode`** — Array of JSON nodes
* **`JSONObjectNode`** — Object of key–value pairs

---

## JSONHandle Convenience Wrapper

`JSONHandle` is a lightweight wrapper around `JSONNodePtr` that provides a safer and more expressive API for navigating and manipulating JSON trees.

It behaves like a **non-owning view** with Python-like ergonomics while preserving strict type semantics.

---

### Safe Navigation (`operator[]`)

```cpp
JSONHandle root = Parser::loads(json_string);

auto value = root["config"]["threads"];
```

* Returns an empty handle if:
  * The node type is incorrect
  * The key or index does not exist
* Never throws

---

### Checked Access (`at()`)

```cpp
auto threads = root.at("config").at("threads");
```

* Throws `std::runtime_error` if:

  * The node is not an object or array
  * The key or index does not exist

---

### Typed Value Extraction (`to<T>()`)

```cpp
int threads = root["threads"].to<int>();
std::string name = root["name"].to<std::string>();
```

* Valid only for value nodes
* Throws if the type does not match
* Returns a default-constructed value if the handle is empty

---

### Range-based Iteration

```cpp
for (JSONHandle child : root["items"]) {
    std::cout << child.str() << "\n";
}
```

* Arrays iterate over elements
* Objects iterate over values (in insertion order)
* Value nodes iterate as empty ranges

---

### Type-safe Casting

```cpp
auto &obj = root.cast<JSONObjectNode>();
auto &arr = root["items"].cast<JSONArrayNode>();
```

* Throws on invalid casts
* Intended for advanced use cases

---

### String Serialization

```cpp
root.str();        // pretty-printed (default)
root.str(false);   // compact
```

`str()` is a convenience wrapper around the parser’s dump functionality.

---

## Parser API

### `Parser::loads`

Parses a JSON string and returns a `JSONHandle`.

### `Parser::dumps`

Serializes a JSON node into a string (pretty or compact).

---

## Notes

* `JSONHandle` is cheap to copy (wraps a `shared_ptr`)
* Invalid access never causes undefined behavior
* Iteration order matches insertion order
* Exit codes from `validate-json` integrate cleanly with CTest

---

For additional examples and details, refer directly to the source files in this repository.
