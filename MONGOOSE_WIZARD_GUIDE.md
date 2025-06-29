# Mongoose Wizard Guide

This guide documents how to create Web UIs for embedded firmware using the Mongoose Wizard tool and its JSON configuration format.

## Overview

Mongoose Wizard is a visual tool that generates C code for REST APIs and web interfaces. You define your API endpoints and UI in JSON, and it generates the glue code to connect your web interface to your firmware.

## JSON Structure

### Basic Template

```json
{
  "version": "1.0.2",  // Required - Wizard schema version
  "level": 0,
  "http": {
    "http": true,
    "https": false,
    "ui": true,
    "login": false
  },
  "mqtt": {
    "enable": false
  },
  "dns": {
    "type": "default",
    "url": "udp://8.8.8.8:53",
    "captive": false
  },
  "sntp": {
    "enable": false
  },
  "modbus": {
    "enable": false
  },
  "build": {
    "board": "teensy41",        // Your target board
    "ide": "PlatformIO",        // Your development environment
    "rtos": "baremetal",
    "cores": [],
    "mode": "existing"
  },
  "api": {
    // API endpoints go here
  },
  "ui": {
    // UI definition goes here
  }
}
```

## API Endpoints

### Data Endpoint
For configuration values that can be read and written:

```json
"config_endpoint": {
  "type": "data",
  "read_level": 0,      // 0-7, who can read
  "write_level": 3,     // 0-7, who can write
  "attributes": {
    "boolParam": {
      "type": "bool",
      "value": true
    },
    "intParam": {
      "type": "int",
      "value": 42,
      "min": 0,
      "max": 100
    },
    "stringParam": {
      "type": "string",
      "value": "default text",
      "size": 50
    }
  }
}
```

### Read-Only Data Endpoint
For status information:

```json
"status_endpoint": {
  "type": "data",
  "read_level": 0,
  "write_level": 7,     // High write level = effectively read-only
  "readonly": true,
  "attributes": {
    "counter": {
      "type": "int",
      "value": 0
    }
  }
}
```

### Action Endpoint
For triggering operations:

```json
"action_endpoint": {
  "type": "action",
  "read_level": 0,
  "write_level": 3,
  "value": false
}
```

### OTA Update Endpoint
For firmware updates:

```json
"firmware_update": {
  "type": "ota",
  "read_level": 3,
  "write_level": 7
}
```

## UI Structure

### Pages/Tabs
Define navigation structure:

```json
"ui": {
  "production": true,
  "brand": "Your Product Name",
  "logo": "",
  "pages": [
    {
      "title": "Settings",
      "icon": "fa-cog"
    },
    {
      "title": "Status", 
      "icon": "fa-info-circle"
    }
  ]
}
```

### UI Elements

UI elements are defined within a `layout` array hierarchy. Each page has a layout that can contain containers, panels, and various UI elements.

#### Layout Structure
```json
"pages": [
  {
    "title": "Page Title",
    "icon": "icon-name",
    "level": 0,  // Access level
    "css": "padding: 0.75rem; gap: 0.5rem; ...",
    "layout": [
      // Elements go here
    ]
  }
]
```

#### Container and Panel Structure
```json
{
  "classes": "container",  // or "panel"
  "css": "width: 50%; margin-bottom: 1rem",  // Optional styling
  "layout": [
    // Nested elements
  ]
}
```

#### Common UI Elements

1. **Title/Label**
```json
{
  "classes": "title",  // or "label"
  "format": "Display Text"
}
```

2. **Toggle Switch**
```json
{
  "classes": "labeled",
  "layout": [
    {"classes": "label", "format": "Toggle Label"},
    {
      "type": "toggle",
      "ref": "api_endpoint.attribute_name",
      "autosave": true
    }
  ]
}
```

3. **Select Dropdown**
```json
{
  "classes": "labeled",
  "layout": [
    {"classes": "label", "format": "Select Label"},
    {
      "type": "select",
      "ref": "api_endpoint.attribute_name",
      "autosave": true,
      "options": {
        "0": "Option Zero",
        "1": "Option One",
        "2": "Option Two"
      }
    }
  ]
}
```

4. **Input Field**
```json
{
  "classes": "labeled",
  "layout": [
    {"classes": "label", "format": "Input Label"},
    {
      "type": "input",
      "ref": "api_endpoint.attribute_name",
      "autosave": true
    }
  ]
}
```

5. **Text Display (Read-only)**
```json
{
  "classes": "labeled",
  "layout": [
    {"classes": "label", "format": "Value Label"},
    {
      "type": "text",
      "ref": "api_endpoint.attribute_name"
    }
  ]
}
```

6. **Button**
```json
{
  "type": "button",
  "ref": "api_endpoint",
  "format": "Button Text",
  "css": "background-color: #3498db; color: white; ..."
}
```

#### Key Points:
- Use `ref` to bind UI elements to API endpoints (not `api` or `bind`)
- Elements are nested within `layout` arrays
- Use `classes` for structural elements (container, panel, title, label, labeled)
- Use `type` for interactive elements (toggle, select, input, button, text)
- `autosave: true` enables automatic saving when values change
- CSS can be applied at any level for custom styling

## Data Types

### Supported API Data Types
- `bool` - Boolean true/false
- `int` - Integer with optional min/max
- `double` - Floating point with optional min/max
- `string` - Text with size limit

### UI Element Types (observed)
- `text` - Static text/labels
- `toggle` - On/off switch
- `select` - Dropdown menu
- `input` - Text/number input field
- `action` - Button that triggers an action
- `container` - Groups other elements

## Integration with Firmware

### Generated Code Structure
The Wizard generates C code that provides:
1. REST API endpoint handlers
2. JSON serialization/deserialization
3. HTTP request routing
4. Data validation

### Connecting to Your Code
You'll need to:
1. Include the generated files in your build
2. Implement getter/setter functions for your parameters
3. Wire up action handlers to your functions
4. Initialize the web server with the generated routes

## Common Patterns

### Configuration Page
```json
{
  "api": {
    "settings": {
      "type": "data",
      "read_level": 0,
      "write_level": 3,
      "attributes": {
        // Your configuration parameters
      }
    }
  }
}
```

### Status Dashboard
```json
{
  "api": {
    "status": {
      "type": "data",
      "read_level": 0,
      "write_level": 7,
      "readonly": true,
      "attributes": {
        // Your status values
      }
    }
  }
}
```

### Control Panel with Actions
```json
{
  "api": {
    "reboot": {
      "type": "action",
      "read_level": 0,
      "write_level": 7,
      "value": false
    },
    "reset_defaults": {
      "type": "action",
      "read_level": 0,
      "write_level": 7,
      "value": false
    }
  }
}
```

## Troubleshooting

### Version Mismatch
- Error: "Unsupported version X.X.X, expected 1.0.2"
- Solution: Set `"version": "1.0.2"` in your JSON

### Empty UI
- Issue: Sidebar shows but no UI elements appear
- Possible causes:
  - Incorrect UI structure format
  - Missing required properties
  - Wrong nesting level

### API Endpoints Not Working
- Check read/write levels match your security model
- Verify attribute types match your firmware
- Ensure attribute names are valid C identifiers

## EventLogger Example (Work in Progress)

This example shows a configuration UI for an event logging system:

```json
{
  "version": "1.0.2",
  "api": {
    "logger_config": {
      "type": "data",
      "read_level": 0,
      "write_level": 3,
      "attributes": {
        "enableSerial": {"type": "bool", "value": true},
        "serialLevel": {"type": "int", "value": 6, "min": 0, "max": 7},
        "enableUDP": {"type": "bool", "value": false},
        "udpLevel": {"type": "int", "value": 4, "min": 0, "max": 7},
        "syslogPort": {"type": "int", "value": 514, "min": 1, "max": 65535}
      }
    },
    "logger_stats": {
      "type": "data",
      "read_level": 0,
      "write_level": 7,
      "readonly": true,
      "attributes": {
        "eventCount": {"type": "int", "value": 0}
      }
    },
    "logger_reset": {
      "type": "action",
      "read_level": 0,
      "write_level": 3,
      "value": false
    }
  }
  // UI structure still being determined
}
```

## Complete Working Example

Here's a minimal working example showing the correct structure:

```json
{
  "version": "1.0.2",
  "api": {
    "leds": {
      "type": "data",
      "attributes": {
        "led1": {"type": "bool", "value": false}
      }
    }
  },
  "ui": {
    "production": false,
    "brand": "Brand Name",
    "logo": "",
    "toolbar": {"label": "My product console"},
    "theme": {},
    "pages": [
      {
        "title": "Dashboard",
        "icon": "desktop",
        "level": 0,
        "css": "padding: 0.75rem; gap: 0.5rem; min-height: 2rem; display: flex; flex-direction: column; flex-grow: 1;",
        "layout": [
          {
            "classes": "container",
            "layout": [
              {
                "classes": "panel",
                "css": "width: 14rem",
                "layout": [
                  {"classes": "title", "format": "LED control panel"},
                  {
                    "classes": "labeled",
                    "layout": [
                      {"classes": "label", "format": "LED1"},
                      {"type": "toggle", "ref": "leds.led1", "autosave": true}
                    ]
                  }
                ]
              }
            ]
          }
        ]
      }
    ]
  },
  "http": {
    "http": true,
    "https": true,
    "ui": true,
    "login": false
  },
  "mqtt": {"enable": false},
  "dns": {
    "type": "default",
    "url": "udp://8.8.8.8:53",
    "captive": false
  },
  "sntp": {"enable": false},
  "modbus": {"enable": false},
  "build": {
    "board": "unix",
    "ide": "GCC+make",
    "rtos": "baremetal",
    "cores": [],
    "mode": "existing"
  }
}
```

## TODO

- [x] Complete UI element structure documentation
- [ ] Add more complex UI element examples
- [ ] Document CSS classes and styling options
- [ ] Add WebSocket configuration for real-time updates
- [ ] Include examples of generated C code
- [ ] Add migration guide from manual REST API implementation

## Resources

- [Mongoose Documentation](https://mongoose.ws/documentation/)
- [Mongoose Wizard](https://mongoose.ws) (requires account)
- [Mongoose GitHub Examples](https://github.com/cesanta/mongoose/tree/master/examples)

---

*Note: This guide is a work in progress. The UI structure format is still being determined through testing.*