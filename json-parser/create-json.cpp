#include "json.hpp"
#include <iostream>

int main() {
   // Array
   JSON::JSONNodePtr one     = JSON::helper::createNode(1);
   JSON::JSONNodePtr two     = JSON::helper::createNode(2);
   JSON::JSONNodePtr three   = JSON::helper::createNode(3);
   JSON::JSONNodePtr arr_    = JSON::helper::createArray("array", {one, two, three});

   // Simple Data types
   JSON::JSONNodePtr bool_   = JSON::helper::createNode("boolean", true);
   JSON::JSONNodePtr null_   = JSON::helper::createNode("null", nullptr);
   JSON::JSONNodePtr int_    = JSON::helper::createNode("number", 123);
   JSON::JSONNodePtr float_  = JSON::helper::createNode("float", 1.0);
   JSON::JSONNodePtr string_ = JSON::helper::createNode("string", "Hello world");

   // Object
   JSON::JSONNodePtr a       = JSON::helper::createNode("a", "b");
   JSON::JSONNodePtr c       = JSON::helper::createNode("c", "d");
   JSON::JSONNodePtr empty   = JSON::helper::createNode("", "empty");
   JSON::JSONNodePtr obj_    = JSON::helper::createObject("object", {a, c, empty});

   // Create the root object
   JSON::JSONNodePtr root    = JSON::helper::createObject({arr_, bool_, null_, int_, float_, string_, obj_});

   // Serialize & print the result
   std::cout << JSON::Parser::dumps(root, true) << "\n";
}
