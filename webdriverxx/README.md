## TODO:
1. Actions API
2. Execute async
3. Improve test cases
4. Better error handling

### Endpoints Supported

| Method  | URI Template                                                    | Command                    | Status  |
|---------|-----------------------------------------------------------------|----------------------------|---------|
| POST    | /session                                                        | New Session                | Done    |
| DELETE  | /session/{session id}                                           | Delete Session             | Done    |
| GET     | /status                                                         | Status                     | Done    |
| GET     | /session/{session id}/timeouts                                  | Get Timeouts               | Done    |
| POST    | /session/{session id}/timeouts                                  | Set Timeouts               | Done    |
| POST    | /session/{session id}/url                                       | Go                         | Done    |
| GET     | /session/{session id}/url                                       | Get Current URL            | Done    |
| POST    | /session/{session id}/back                                      | Back                       | Done    |
| POST    | /session/{session id}/forward                                   | Forward                    | Done    |
| POST    | /session/{session id}/refresh                                   | Refresh                    | Done    |
| GET     | /session/{session id}/title                                     | Get Title                  | Done    |
| GET     | /session/{session id}/window                                    | Get Window Handle          | Done    |
| DELETE  | /session/{session id}/window                                    | Close Window               | Done    |
| POST    | /session/{session id}/window                                    | Switch To Window           | Done    |
| GET     | /session/{session id}/window/handles                            | Get Window Handles         | Done    |
| GET     | /session/{session id}/window/new                                | Create new Window / Tab    | Done    |
| POST    | /session/{session id}/frame                                     | Switch To Frame            | Done    |
| POST    | /session/{session id}/frame/parent                              | Switch To Parent Frame     | Done    |
| GET     | /session/{session id}/window/rect                               | Get Window Rect            | Done    |
| POST    | /session/{session id}/window/rect                               | Set Window Rect            | Done    |
| POST    | /session/{session id}/window/maximize                           | Maximize Window            | Done    |
| POST    | /session/{session id}/window/minimize                           | Minimize Window            | Done    |
| POST    | /session/{session id}/window/fullscreen                         | Fullscreen Window          | Done    |
| POST    | /session/{session id}/element                                   | Find Element               | Done    |
| POST    | /session/{session id}/elements                                  | Find Elements              | Done    |
| POST    | /session/{session id}/element/{element id}/element              | Find Element From Element  | Done    |
| POST    | /session/{session id}/element/{element id}/elements             | Find Elements From Element | Done    |
| GET     | /session/{session id}/element/active                            | Get Active Element         | Done    |
| GET     | /session/{session id}/element/{element id}/selected             | Is Element Selected        | Done    |
| GET     | /session/{session id}/element/{element id}/attribute/{name}     | Get Element Attribute      | Done    |
| GET     | /session/{session id}/element/{element id}/property/{name}      | Get Element Property       | Done    |
| GET     | /session/{session id}/element/{element id}/css/{property name}  | Get Element CSS Value      | Done    |
| GET     | /session/{session id}/element/{element id}/text                 | Get Element Text           | Done    |
| GET     | /session/{session id}/element/{element id}/name                 | Get Element Tag Name       | Done    |
| GET     | /session/{session id}/element/{element id}/rect                 | Get Element Rect           | Done    |
| GET     | /session/{session id}/element/{element id}/enabled              | Is Element Enabled         | Done    |
| POST    | /session/{session id}/element/{element id}/click                | Element Click              | Done    |
| POST    | /session/{session id}/element/{element id}/clear                | Element Clear              | Done    |
| POST    | /session/{session id}/element/{element id}/value                | Element Send Keys          | Done    |
| GET     | /session/{session id}/source                                    | Get Page Source            | Done    |
| POST    | /session/{session id}/execute/sync                              | Execute Script             | Done    |
| POST    | /session/{session id}/execute/async                             | Execute Async Script       |         |
| GET     | /session/{session id}/cookie                                    | Get All Cookies            | Done    |
| GET     | /session/{session id}/cookie/{name}                             | Get Named Cookie           | Done    |
| POST    | /session/{session id}/cookie                                    | Add Cookie                 | Done    |
| DELETE  | /session/{session id}/cookie/{name}                             | Delete Cookie              | Done    |
| DELETE  | /session/{session id)/cookie                                    | Delete All Cookies         | Done    |
| POST    | /session/{session id}/actions                                   | Perform Actions            |         |
| DELETE  | /session/{session id}/actions                                   | Release Actions            |         |
| POST    | /session/{session id}/alert/dismiss                             | Dismiss Alert              | Done    |
| POST    | /session/{session id}/alert/accept                              | Accept Alert               | Done    |
| GET     | /session/{session id}/alert/text                                | Get Alert Text             | Done    |
| POST    | /session/{session id}/alert/text                                | Send Alert Text            | Done    |
| GET     | /session/{session id}/screenshot                                | Take Screenshot            | Done    |
| GET     | /session/{session id}/element/{element id}/screenshot           | Take Element Screenshot    | Done    |
| POST    | /session/{session id}/print                                     | Print Page                 | Done    |
