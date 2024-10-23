# JSON Parser and Validator

This project offers a lightweight, header-only JSON library in C++ (`json.hpp`) that enables easy creation, parsing, and validation of JSON documents. It also includes three example programs: one for validating JSON files (`validate-json.cpp`), another for generating sample JSON documents (`create-json.cpp`), and a third for manipulating existing JSON data (`manipulate-json.cpp`)

### Additional Information
- **Inspiration**: [Coding Challenges JSON Parser](https://codingchallenges.fyi/challenges/challenge-json-parser)
- **Data Structure Credits**: [Stack Overflow on Data Types for Representing JSON in C](https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c)
  
### Design Decisions
1. Mimic (imperfectly) Python's JSON module with `loads` and `dumps` methods.
2. Throw errors instead of using failing silently by returning a `nullptr`.
3. Use `std::vector` for both arrays and objects to maintain insertion order, following insights from the Stack Overflow thread.
4. Allow arbitrary nesting depth: The parser is designed to support arbitrary levels of nesting. This means that test cases like `fail18.json`, which may contain deep nesting, are validated as correct.

## Requirements

- **Compiler**: g++ (with C++23 support)
- **Make**: for building and running tests

## Project Structure

- **json.hpp**: Header-only library for creating and parsing JSON.
- **validate-json.cpp**: Program that validates JSON files, given a path argument. In addition to validation, it benchmarks the runtime (in milliseconds) for processing each JSON file. This can help assess performance with files of varying sizes.
- **create-json.cpp**: Example program that demonstrates creating a JSON document using the library.
- **manipulate-json.cpp**: Demonstrates loading a JSON document, casting nodes to object references, modifying keys, and adding new elements.
- **test/**: Directory containing sample JSON files (`passXX.json`, `failXX.json`, `benchxx.json`) for validation. Includes 318 additional tests from the [JSONTestSuite](https://github.com/nst/JSONTestSuite/tree/master/test_parsing) under `test/extras/`.

## Building the Project

Run the following command to build both example programs:

```bash
make
```

This will produce three executables in the `build/` directory:
- `build/validate-json.out`
- `build/create-json.out`
- `build/manipulate-json.out`

## Running the JSON Validator

To validate JSON files in the `test/` directory, run:

```bash
make test
```

This will execute `validate-json.out` on the test files and print the results to the console. Tests under `test/extras` are not included by default and must be run manually if desired.

## Creating Sample JSON Documents

The `create-json.cpp` demonstrates how to create JSON structures using the library.

### Example Usage:
```cpp
// Creating an array
JSON::JSONNode_Ptr one   = JSON::helper::createNode(1);
JSON::JSONNode_Ptr two   = JSON::helper::createNode(2);
JSON::JSONNode_Ptr arr   = JSON::helper::createArray("array", {one, two});

// Simple key-value pairs
JSON::JSONNode_Ptr int_  = JSON::helper::createNode("number", 123);
JSON::JSONNode_Ptr str_  = JSON::helper::createNode("string", "Hello World");

// Create a root object
JSON::JSONNode_Ptr root  = JSON::helper::createObject({arr, int_, str_});

// Serialize to JSON string
std::cout << JSON::Parser::dumps(root) << "\n";
```

```json
{"array": [1, 2], "number": 123, "string": "Hello World"}
```

## Definitions

### Types of JSON Objects
The library defines several classes to represent different JSON structures:

- **`JSONNode`**: Base class for all JSON nodes, handling keys and types.
- **`JSONValueNode`**: Represents simple key-value pairs.
- **`JSONArrayNode`**: Represents an array of JSON nodes, equivalent to JSON arrays.
- **`JSONObjectNode`**: Represents an object with key-value pairs, equivalent to JSON objects.

### Helper Functions
The `helper` namespace provides functions to simplify the creation of JSON nodes and structures, making it easy to build complex JSON documents.

#### Example Helper Functions:
- **`createNode`**: Creates a simple JSON node from a value.
- **`createArray`**: Creates an array node containing other JSON nodes.
- **`createObject`**: Creates an object node with key-value pairs.
- **`pretty`**: Prettifies a JSON dump string by formatting it with appropriate indentation for better readability.

### Parser Class
The `Parser` class handles the logic to parse JSON from strings and to dump JSON into strings.

#### Key Methods:
- **`loads`**: Loads JSON from a string in memory.
- **`dumps`**: Serializes a JSON node to a string.

For detailed definitions and more examples, refer to the source files included in this project.
