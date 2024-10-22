#include "json.hpp"
#include <iostream>

int main() {
   // Array
   JSON::JSONNode_Ptr one     = JSON::helper::createNode(1);
   JSON::JSONNode_Ptr two     = JSON::helper::createNode(2);
   JSON::JSONNode_Ptr three   = JSON::helper::createNode(3);
   JSON::JSONNode_Ptr arr_    = JSON::helper::createArray("array", {one, two, three});

   // Simple Data types
   JSON::JSONNode_Ptr bool_   = JSON::helper::createNode("boolean", true);
   JSON::JSONNode_Ptr null_   = JSON::helper::createNode("null", nullptr);
   JSON::JSONNode_Ptr int_    = JSON::helper::createNode("number", 123);
   JSON::JSONNode_Ptr float_  = JSON::helper::createNode("float", 1.0);
   JSON::JSONNode_Ptr string_ = JSON::helper::createNode("string", "Hello world");

   // Object
   JSON::JSONNode_Ptr a       = JSON::helper::createNode("a", "b");
   JSON::JSONNode_Ptr c       = JSON::helper::createNode("c", "d");
   JSON::JSONNode_Ptr empty   = JSON::helper::createNode("", "empty");
   JSON::JSONNode_Ptr obj_    = JSON::helper::createObject("object", {a, c, empty});

   // Create the root object
   JSON::JSONNode_Ptr root    = JSON::helper::createObject({arr_, bool_, null_, int_, float_, string_, obj_});

   // Serialize & print the result
   std::cout << JSON::Parser::dumps(root) << "\n";
}
