# JSON Parser and Validator

This project provides a simple C++ header-only JSON library (`json.hpp`) that can be used to create, parse, and validate JSON documents. It also includes two example programs: one for validating JSON files (`validate-json.cpp`) and another for generating sample JSON documents (`example-document.cpp`).

### Additional Information
- **Inspiration**: [Coding Challenges JSON Parser](https://codingchallenges.fyi/challenges/challenge-json-parser)
- **Data Structure Credits**: [Stack Overflow on Data Types for Representing JSON in C](https://stackoverflow.com/questions/19543326/datatypes-for-representing-json-in-c)
  
### Design Decisions
1. Mimic (imperfectly) Python's JSON module with `loads` and `dumps` methods.
2. Throw errors instead of using `cout` with `nullptr`.
3. Use `std::vector` for both arrays and objects to maintain insertion order, following insights from the Stack Overflow thread.'

## Requirements

- **Compiler**: g++ (with C++23 support)
- **Make**: for building and running tests

## Project Structure

- **json.hpp**: Header-only library for creating and parsing JSON.
- **validate-json.cpp**: Program that validates JSON files.
- **example-document.cpp**: Example program that demonstrates creating a JSON document using the library.
- **test/**: Directory containing sample JSON files (`passXX.json`, `failXX.json`) for validation.

## Building the Project

Run the following command to build both example programs:

```bash
make
```

This will produce two executables in the `build/` directory:
- `build/validate-json.out`
- `build/example-document.out`

## Running the JSON Validator

To validate JSON files in the `test/` directory, run:

```bash
make test
```

This will execute `validate-json.out` on the test files and print the results to the console.

## Creating Sample JSON Documents

The `example-document.cpp` demonstrates how to create JSON structures using the library.

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

## Definitions

### Types of JSON Objects
The library defines several classes to represent different JSON structures:

- **`JSONNode`**: Base class for all JSON nodes, handling keys and types.
- **`JSONValueNode`**: Represents simple key-value pairs.
- **`JSONArrayNode`**: Represents an array of JSON nodes, allowing for nested structures.
- **`JSONObjectNode`**: Represents an object with key-value pairs, ensuring no duplicate keys.

### Helper Functions
The `helper` namespace provides functions to simplify the creation of JSON nodes and structures, making it easy to build complex JSON documents.

#### Example Helper Functions:
- **`createNode`**: Creates a simple JSON node from a value.
- **`createArray`**: Creates an array node containing other JSON nodes.
- **`createObject`**: Creates an object node with key-value pairs.

### Parser Class
The `Parser` class handles the logic to parse JSON from strings and to dump JSON into strings.

#### Key Methods:
- **`loads`**: Loads JSON from a string in memory.
- **`dumps`**: Serializes a JSON node to a string.

For detailed definitions and more examples, refer to the source files included in this project.
