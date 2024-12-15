### Design Patterns in C++

This repository contains implementations of various **Design Patterns** in C++ categorized into three main types: **Creational**, **Structural**, and **Behavioral**. These patterns are fundamental in object-oriented design and help solve recurring design problems in software development.

---

### Folder Structure

1. **`behavioral-patterns/`**  
   Patterns that deal with communication between objects and the delegation of responsibilities.

2. **`creational-patterns/`**  
   Patterns that focus on object creation mechanisms to increase flexibility and reuse.

3. **`structural-patterns/`**  
   Patterns that deal with the composition of classes and objects to form larger structures.

Each folder contains a **README** file with a detailed description of the patterns, alongside C++ implementations of the respective patterns.

---

### What Are Design Patterns?

Design patterns are typical solutions to common problems in software design. They are like templates designed to help you write code that is easier to understand, maintain, and reuse. Design patterns are categorized as:

1. **Creational Patterns**  
   _Focus_: Object creation mechanisms.  
   _Examples_: Singleton, Factory, Builder, Prototype, Abstract Factory.

2. **Structural Patterns**  
   _Focus_: Relationships between objects to form larger systems.  
   _Examples_: Adapter, Composite, Decorator, Facade, Flyweight, Proxy, Bridge.

3. **Behavioral Patterns**  
   _Focus_: Communication between objects and encapsulating behavior.  
   _Examples_: Strategy, Observer, Visitor, Template, State, Command, Chain of Responsibility.

---

### How to Use

Each pattern is implemented in a separate C++ file inside its corresponding folder. You can compile and run these files to see how the patterns work. Below is a quick start guide:

1. Clone the repository:  
   ```bash
   git clone https://github.com/infinage/cpp-experiments
   cd cpp-experiments/design-patterns
   ```

2. Navigate to the desired folder:  
   ```bash
   cd behavioral-patterns
   ```

3. Compile and run the desired pattern:  
   ```bash
   g++ observer.cpp -o observer -std=c++23
   ./observer
   ```

---

### Why Learn Design Patterns?

- **Enhance Code Quality**: Write clean, maintainable, and scalable code.  
- **Increase Reusability**: Solve common problems with reusable solutions.  
- **Improve Communication**: Share ideas effectively using pattern terminology.

Whether you're preparing for interviews, improving your design skills, or building robust systems, understanding design patterns is essential for any software developer.
