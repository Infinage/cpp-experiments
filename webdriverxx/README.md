# WebDriver++

WebDriver++ is a lightweight C++ client for Selenium WebDriver, designed to automate web browsers programmatically. It provides a simple and intuitive API to interact with browsers via WebDriver.

---

## Features
- **Lightweight and minimal dependencies**: Uses `libcpr/cpr` and `nlohmann/json`.
- **Header-only library**: Easy to integrate into your projects.
- **RAII-based session management**: Ensures proper resource cleanup.
- **Improved error handling**: Detailed request/response logs for debugging.
- **Cross-browser support**: Works with Chrome, Firefox, and Edge.

---

## Getting Started

### Installation
To use WebDriver++, clone the repository and build it using CMake:

```sh
git clone <repo-url>
cd webdriverxx
mkdir build && cd build
cmake ..
```

---

### Usage
```cpp
// example.cpp
#include "webdriverxx.hpp"

int main() {
    // Initialize the WebDriver with browser capabilities and port
    webdriverxx::Driver driver{webdriverxx::Capabilities{BROWSER_TYPE, BROWSER_BINARY}, PORT};

    // Navigate to a webpage
    driver.navigateTo("https://duckduckgo.com");

    // Find an element and interact with it
    webdriverxx::Element element{driver.findElement(webdriverxx::LOCATION_STRATEGY::CSS, "#searchbox_input")};
    element.sendKeys("Hello world").submit();

    return 0;
}
```

#### Compile and Run:
```sh
g++ example.cpp -o example -std=c++21 -Iinclude
./example
```

For more examples, check the `examples` directory in the repository.

---

## TODO
- [ ] Implement Actions API
- [ ] Add support for Execute Async
- [ ] Expand test coverage with additional test cases

---

## Running Tests
WebDriver++ includes a comprehensive set of test cases. Follow the steps below to run the tests:

1. Modify `CMakeLists.txt` under the `_tests_` directory to specify the correct `BINARY_PATH` for your browser executables.
2. Run the tests using the following commands:

#### Run All Tests (Firefox, Chrome, Edge):
```sh
FIREFOX_PORT=1001 CHROME_PORT=1002 MSEDGE_PORT=1003 ctest --output-on-failure
```

#### Run Tests for a Single Browser:
```sh
CHROME_PORT=1002 ctest -L chrome --output-on-failure
```

#### Run Tests Matching a Specific Pattern:
```sh
CHROME_PORT=1002 ctest -R '*find*CHROME' --output-on-failure
```

---

## Supported Endpoints
WebDriver++ supports a wide range of WebDriver API endpoints.

---

### Session Management
| Method  | URI Template                      | Command          | Status  |
|---------|-----------------------------------|------------------|---------|
| POST    | /session                          | New Session      | ✅      |
| DELETE  | /session/{session id}             | Delete Session   | ✅      |

---

### Status
| Method  | URI Template                      | Command          | Status  |
|---------|-----------------------------------|------------------|---------|
| GET     | /status                           | Status           | ✅      |

---

### Timeouts
| Method  | URI Template                      | Command          | Status  |
|---------|-----------------------------------|------------------|---------|
| GET     | /session/{session id}/timeouts    | Get Timeouts     | ✅      |
| POST    | /session/{session id}/timeouts    | Set Timeouts     | ✅      |

---

### Navigation
| Method  | URI Template                      | Command          | Status  |
|---------|-----------------------------------|------------------|---------|
| POST    | /session/{session id}/url         | Go               | ✅      |
| GET     | /session/{session id}/url         | Get Current URL  | ✅      |
| POST    | /session/{session id}/back        | Back             | ✅      |
| POST    | /session/{session id}/forward     | Forward          | ✅      |
| POST    | /session/{session id}/refresh     | Refresh          | ✅      |

---

### Window Management
| Method  | URI Template                            | Command                 | Status  |
|---------|-----------------------------------------|-------------------------|---------|
| GET     | /session/{session id}/window            | Get Window Handle       | ✅      |
| DELETE  | /session/{session id}/window            | Close Window            | ✅      |
| POST    | /session/{session id}/window            | Switch To Window        | ✅      |
| GET     | /session/{session id}/window/handles    | Get Window Handles      | ✅      |
| GET     | /session/{session id}/window/new        | Create new Window / Tab | ✅      |
| GET     | /session/{session id}/window/rect       | Get Window Rect         | ✅      |
| POST    | /session/{session id}/window/rect       | Set Window Rect         | ✅      |
| POST    | /session/{session id}/window/maximize   | Maximize Window         | ✅      |
| POST    | /session/{session id}/window/minimize   | Minimize Window         | ✅      |
| POST    | /session/{session id}/window/fullscreen | Fullscreen Window       | ✅      |

---

### Frame Management
| Method  | URI Template                      | Command                | Status  |
|---------|-----------------------------------|------------------------|---------|
| POST    | /session/{session id}/frame       | Switch To Frame        | ✅      |
| POST    | /session/{session id}/frame/parent| Switch To Parent Frame | ✅      |

---

### Element Interaction
| Method  | URI Template                                                   | Command                    | Status  |
|---------|----------------------------------------------------------------|----------------------------|---------|
| POST    | /session/{session id}/element                                  | Find Element               | ✅      |
| POST    | /session/{session id}/elements                                 | Find Elements              | ✅      |
| POST    | /session/{session id}/element/{element id}/element             | Find Element From Element  | ✅      |
| POST    | /session/{session id}/element/{element id}/elements            | Find Elements From Element | ✅      |
| GET     | /session/{session id}/element/active                           | Get Active Element         | ✅      |
| GET     | /session/{session id}/element/{element id}/selected            | Is Element Selected        | ✅      |
| GET     | /session/{session id}/element/{element id}/attribute/{name}    | Get Element Attribute      | ✅      |
| GET     | /session/{session id}/element/{element id}/property/{name}     | Get Element Property       | ✅      |
| GET     | /session/{session id}/element/{element id}/css/{property name} | Get Element CSS Value      | ✅      |
| GET     | /session/{session id}/element/{element id}/text                | Get Element Text           | ✅      |
| GET     | /session/{session id}/element/{element id}/name                | Get Element Tag Name       | ✅      |
| GET     | /session/{session id}/element/{element id}/rect                | Get Element Rect           | ✅      |
| GET     | /session/{session id}/element/{element id}/enabled             | Is Element Enabled         | ✅      |
| POST    | /session/{session id}/element/{element id}/click               | Element Click              | ✅      |
| POST    | /session/{session id}/element/{element id}/clear               | Element Clear              | ✅      |
| POST    | /session/{session id}/element/{element id}/value               | Element Send Keys          | ✅      |

---

### Page Information
| Method  | URI Template                      | Command                | Status  |
|---------|-----------------------------------|------------------------|---------|
| GET     | /session/{session id}/source      | Get Page Source        | ✅      |
| GET     | /session/{session id}/title       | Get Title              | ✅      |

---

### Script Execution
| Method  | URI Template                        | Command                | Status  |
|---------|-------------------------------------|------------------------|---------|
| POST    | /session/{session id}/execute/sync  | Execute Script         | ✅      |
| POST    | /session/{session id}/execute/async | Execute Async Script   |         |

---

### Cookie Management
| Method  | URI Template                        | Command                | Status  |
|---------|-------------------------------------|------------------------|---------|
| GET     | /session/{session id}/cookie        | Get All Cookies        | ✅      |
| GET     | /session/{session id}/cookie/{name} | Get Named Cookie       | ✅      |
| POST    | /session/{session id}/cookie        | Add Cookie             | ✅      |
| DELETE  | /session/{session id}/cookie/{name} | Delete Cookie          | ✅      |
| DELETE  | /session/{session id}/cookie        | Delete All Cookies     | ✅      |

---

### Actions
| Method  | URI Template                      | Command                | Status  |
|---------|-----------------------------------|------------------------|---------|
| POST    | /session/{session id}/actions     | Perform Actions        |         |
| DELETE  | /session/{session id}/actions     | Release Actions        |         |

---

### Alert Handling
| Method  | URI Template                        | Command                | Status  |
|---------|-------------------------------------|------------------------|---------|
| POST    | /session/{session id}/alert/dismiss | Dismiss Alert          | ✅      |
| POST    | /session/{session id}/alert/accept  | Accept Alert           | ✅      |
| GET     | /session/{session id}/alert/text    | Get Alert Text         | ✅      |
| POST    | /session/{session id}/alert/text    | Send Alert Text        | ✅      |

---

### Screenshots
| Method  | URI Template                                          | Command                 | Status  |
|---------|-------------------------------------------------------|-------------------------|---------|
| GET     | /session/{session id}/screenshot                      | Take Screenshot         | ✅      |
| GET     | /session/{session id}/element/{element id}/screenshot | Take Element Screenshot | ✅      |

---

### Printing
| Method  | URI Template                      | Command                | Status  |
|---------|-----------------------------------|------------------------|---------|
| POST    | /session/{session id}/print       | Print Page             | ✅      |

---

## Contributing
Contributions are welcome! Feel free to open an issue or submit a pull request.

## License
This project is licensed under the MIT License.
