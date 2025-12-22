#ifndef OPENAPI_SPEC_H
#define OPENAPI_SPEC_H

/**
 * @file OpenApiSpec.h
 * @brief OpenAPI 3.0.3 specification for LightwaveOS API v1
 *
 * Provides machine-readable API documentation stored in PROGMEM.
 * Total size: ~9KB (flash only, 0 bytes RAM)
 *
 * Access via: GET /api/v1/spec
 */

#include <Arduino.h>

const char OPENAPI_SPEC[] PROGMEM = R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "LightwaveOS API",
    "version": "1.0",
    "description": "ESP32-S3 LED control system for dual 160-LED WS2812 strips with 45+ visual effects, multi-zone composition, and CENTER ORIGIN design philosophy.",
    "contact": {
      "name": "LightwaveOS",
      "url": "http://lightwaveos.local"
    }
  },
  "servers": [
    {
      "url": "http://lightwaveos.local/api/v1",
      "description": "LightwaveOS Device (mDNS)"
    },
    {
      "url": "http://{deviceIP}/api/v1",
      "description": "LightwaveOS Device (Direct IP)",
      "variables": {
        "deviceIP": {
          "default": "192.168.1.100",
          "description": "Device IP address"
        }
      }
    }
  ],
  "paths": {
    "/": {
      "get": {
        "summary": "API Discovery",
        "description": "Returns HATEOAS links, hardware information, and API capabilities.",
        "tags": ["Discovery"],
        "responses": {
          "200": {
            "description": "API discovery information",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/SuccessResponse"
                },
                "example": {
                  "success": true,
                  "data": {
                    "name": "LightwaveOS",
                    "apiVersion": "1.0",
                    "description": "ESP32-S3 LED Control System",
                    "hardware": {
                      "ledsTotal": 320,
                      "strips": 2,
                      "centerPoint": 79,
                      "maxZones": 4
                    },
                    "_links": {
                      "self": "/api/v1/",
                      "device": "/api/v1/device/status",
                      "effects": "/api/v1/effects",
                      "parameters": "/api/v1/parameters",
                      "transitions": "/api/v1/transitions/types",
                      "batch": "/api/v1/batch",
                      "websocket": "ws://lightwaveos.local/ws"
                    }
                  },
                  "timestamp": 1234567890,
                  "version": "1.0"
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/device/status": {
      "get": {
        "summary": "Device Status",
        "description": "Returns uptime, heap memory, network status, and WebSocket client count.",
        "tags": ["Device"],
        "responses": {
          "200": {
            "description": "Current device status",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/DeviceStatus"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/device/info": {
      "get": {
        "summary": "Device Information",
        "description": "Returns firmware version, board type, SDK version, and flash memory details.",
        "tags": ["Device"],
        "responses": {
          "200": {
            "description": "Device information",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/DeviceInfo"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/effects": {
      "get": {
        "summary": "List Effects",
        "description": "Returns paginated list of available effects with optional metadata details.",
        "tags": ["Effects"],
        "parameters": [
          {
            "name": "start",
            "in": "query",
            "description": "Starting index for pagination",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "default": 0
            }
          },
          {
            "name": "count",
            "in": "query",
            "description": "Number of effects to return",
            "schema": {
              "type": "integer",
              "minimum": 1,
              "maximum": 50,
              "default": 20
            }
          },
          {
            "name": "details",
            "in": "query",
            "description": "Include detailed metadata (features, descriptions)",
            "schema": {
              "type": "boolean"
            }
          }
        ],
        "responses": {
          "200": {
            "description": "List of effects",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/EffectsList"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/effects/current": {
      "get": {
        "summary": "Current Effect",
        "description": "Returns currently active effect and its parameters.",
        "tags": ["Effects"],
        "responses": {
          "200": {
            "description": "Current effect information",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/CurrentEffect"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/effects/metadata": {
      "get": {
        "summary": "Effect Metadata",
        "description": "Returns detailed metadata for a specific effect including category, features, and description.",
        "tags": ["Effects"],
        "parameters": [
          {
            "name": "id",
            "in": "query",
            "description": "Effect ID",
            "required": true,
            "schema": {
              "type": "integer",
              "minimum": 0
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Effect metadata",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/EffectMetadata"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/effects/set": {
      "post": {
        "summary": "Set Effect",
        "description": "Activates a specific effect by ID with optional transition.",
        "tags": ["Effects"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["effectId"],
                "properties": {
                  "effectId": {
                    "type": "integer",
                    "minimum": 0,
                    "description": "Effect ID to activate"
                  }
                }
              },
              "example": {
                "effectId": 5
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Effect activated successfully",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "effectId": {
                              "type": "integer"
                            },
                            "name": {
                              "type": "string"
                            }
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/parameters": {
      "get": {
        "summary": "Get Parameters",
        "description": "Returns all visual parameters (brightness, speed, palette, intensity, etc.).",
        "tags": ["Parameters"],
        "responses": {
          "200": {
            "description": "Current parameter values",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/Parameters"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "post": {
        "summary": "Set Parameters",
        "description": "Updates one or more visual parameters. All fields are optional.",
        "tags": ["Parameters"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "$ref": "#/components/schemas/Parameters"
              },
              "example": {
                "brightness": 200,
                "speed": 25,
                "paletteId": 3
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Parameters updated successfully",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/SuccessResponse"
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/transitions/types": {
      "get": {
        "summary": "Transition Types",
        "description": "Returns list of available transition types and easing curves.",
        "tags": ["Transitions"],
        "responses": {
          "200": {
            "description": "Transition types and easing curves",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/TransitionTypes"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/transitions/config": {
      "get": {
        "summary": "Transition Configuration",
        "description": "Returns current transition settings.",
        "tags": ["Transitions"],
        "responses": {
          "200": {
            "description": "Transition configuration",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "$ref": "#/components/schemas/TransitionConfig"
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "post": {
        "summary": "Update Transition Configuration",
        "description": "Updates transition settings (e.g., enable/disable random transitions).",
        "tags": ["Transitions"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "enabled": {
                    "type": "boolean",
                    "description": "Enable random transitions between effects"
                  }
                }
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Configuration updated successfully",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/SuccessResponse"
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/transitions/trigger": {
      "post": {
        "summary": "Trigger Transition",
        "description": "Manually triggers a transition to a specific effect with optional type, duration, and easing.",
        "tags": ["Transitions"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["toEffect"],
                "properties": {
                  "toEffect": {
                    "type": "integer",
                    "minimum": 0,
                    "description": "Target effect ID"
                  },
                  "type": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 11,
                    "description": "Transition type ID (0-11)"
                  },
                  "duration": {
                    "type": "integer",
                    "minimum": 100,
                    "maximum": 5000,
                    "description": "Transition duration in milliseconds"
                  },
                  "easing": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 14,
                    "description": "Easing curve ID (0-14)"
                  }
                }
              },
              "example": {
                "toEffect": 10,
                "type": 5,
                "duration": 1000,
                "easing": 3
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Transition triggered successfully",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "effectId": {
                              "type": "integer"
                            },
                            "name": {
                              "type": "string"
                            },
                            "transitionType": {
                              "type": "integer"
                            },
                            "duration": {
                              "type": "integer"
                            }
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/batch": {
      "post": {
        "summary": "Batch Operations",
        "description": "Executes multiple operations atomically. Maximum 10 operations per batch.",
        "tags": ["Batch"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["operations"],
                "properties": {
                  "operations": {
                    "type": "array",
                    "minItems": 1,
                    "maxItems": 10,
                    "items": {
                      "$ref": "#/components/schemas/BatchOperation"
                    }
                  }
                }
              },
              "example": {
                "operations": [
                  {
                    "action": "setBrightness",
                    "value": 200
                  },
                  {
                    "action": "setEffect",
                    "effectId": 5
                  },
                  {
                    "action": "setSpeed",
                    "value": 30
                  }
                ]
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Batch operations executed",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {
                      "$ref": "#/components/schemas/SuccessResponse"
                    },
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "processed": {
                              "type": "integer",
                              "description": "Number of successful operations"
                            },
                            "failed": {
                              "type": "integer",
                              "description": "Number of failed operations"
                            },
                            "results": {
                              "type": "array",
                              "items": {
                                "type": "object",
                                "properties": {
                                  "action": {
                                    "type": "string"
                                  },
                                  "success": {
                                    "type": "boolean"
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "400": {
            "$ref": "#/components/responses/ValidationError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    }
  },
  "components": {
    "schemas": {
      "SuccessResponse": {
        "type": "object",
        "required": ["success", "timestamp", "version"],
        "properties": {
          "success": {
            "type": "boolean",
            "enum": [true]
          },
          "data": {
            "type": "object",
            "description": "Response payload (varies by endpoint)"
          },
          "timestamp": {
            "type": "integer",
            "description": "Server timestamp in milliseconds"
          },
          "version": {
            "type": "string",
            "description": "API version"
          }
        }
      },
      "ErrorResponse": {
        "type": "object",
        "required": ["success", "error", "timestamp", "version"],
        "properties": {
          "success": {
            "type": "boolean",
            "enum": [false]
          },
          "error": {
            "type": "object",
            "required": ["code", "message"],
            "properties": {
              "code": {
                "type": "string",
                "enum": [
                  "INVALID_JSON",
                  "MISSING_FIELD",
                  "INVALID_VALUE",
                  "INVALID_TYPE",
                  "OUT_OF_RANGE",
                  "RATE_LIMITED",
                  "NOT_FOUND"
                ],
                "description": "Machine-readable error code"
              },
              "message": {
                "type": "string",
                "description": "Human-readable error message"
              },
              "field": {
                "type": "string",
                "description": "Field name that caused the error (if applicable)"
              }
            }
          },
          "timestamp": {
            "type": "integer",
            "description": "Server timestamp in milliseconds"
          },
          "version": {
            "type": "string",
            "description": "API version"
          }
        }
      },
      "DeviceStatus": {
        "type": "object",
        "properties": {
          "uptime": {
            "type": "integer",
            "description": "Device uptime in seconds"
          },
          "freeHeap": {
            "type": "integer",
            "description": "Free heap memory in bytes"
          },
          "heapSize": {
            "type": "integer",
            "description": "Total heap size in bytes"
          },
          "cpuFreq": {
            "type": "integer",
            "description": "CPU frequency in MHz"
          },
          "network": {
            "type": "object",
            "properties": {
              "connected": {
                "type": "boolean",
                "description": "WiFi connection status"
              },
              "apMode": {
                "type": "boolean",
                "description": "Access point mode active"
              },
              "ip": {
                "type": "string",
                "description": "IP address (if connected)"
              },
              "rssi": {
                "type": "integer",
                "description": "WiFi signal strength in dBm (if connected)"
              }
            }
          },
          "wsClients": {
            "type": "integer",
            "description": "Number of active WebSocket clients"
          }
        }
      },
      "DeviceInfo": {
        "type": "object",
        "properties": {
          "firmware": {
            "type": "string",
            "description": "Firmware version"
          },
          "board": {
            "type": "string",
            "description": "Board model"
          },
          "sdk": {
            "type": "string",
            "description": "ESP-IDF SDK version"
          },
          "flashSize": {
            "type": "integer",
            "description": "Flash chip size in bytes"
          },
          "sketchSize": {
            "type": "integer",
            "description": "Sketch size in bytes"
          },
          "freeSketch": {
            "type": "integer",
            "description": "Free sketch space in bytes"
          }
        }
      },
      "EffectsList": {
        "type": "object",
        "properties": {
          "total": {
            "type": "integer",
            "description": "Total number of effects"
          },
          "start": {
            "type": "integer",
            "description": "Starting index of this page"
          },
          "count": {
            "type": "integer",
            "description": "Number of effects in this page"
          },
          "effects": {
            "type": "array",
            "items": {
              "$ref": "#/components/schemas/Effect"
            }
          },
          "categories": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {
                  "type": "integer"
                },
                "name": {
                  "type": "string"
                }
              }
            }
          }
        }
      },
      "Effect": {
        "type": "object",
        "properties": {
          "id": {
            "type": "integer",
            "description": "Effect ID"
          },
          "name": {
            "type": "string",
            "description": "Effect name"
          },
          "category": {
            "type": "string",
            "description": "Category name"
          },
          "categoryId": {
            "type": "integer",
            "description": "Category ID"
          },
          "centerOrigin": {
            "type": "boolean",
            "description": "Effect originates from center point (LED 79/80)"
          },
          "description": {
            "type": "string",
            "description": "Effect description (only if details=true)"
          },
          "usesSpeed": {
            "type": "boolean",
            "description": "Effect responds to speed parameter (only if details=true)"
          },
          "usesPalette": {
            "type": "boolean",
            "description": "Effect uses color palette (only if details=true)"
          },
          "zoneAware": {
            "type": "boolean",
            "description": "Effect works with zone system (only if details=true)"
          },
          "dualStrip": {
            "type": "boolean",
            "description": "Designed for dual-strip hardware (only if details=true)"
          },
          "physicsBased": {
            "type": "boolean",
            "description": "Uses physics simulation (only if details=true)"
          }
        }
      },
      "CurrentEffect": {
        "type": "object",
        "properties": {
          "effectId": {
            "type": "integer",
            "description": "Current effect ID"
          },
          "name": {
            "type": "string",
            "description": "Current effect name"
          },
          "brightness": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Current brightness"
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50,
            "description": "Current speed"
          },
          "paletteId": {
            "type": "integer",
            "description": "Current palette ID"
          }
        }
      },
      "EffectMetadata": {
        "type": "object",
        "properties": {
          "effectId": {
            "type": "integer"
          },
          "name": {
            "type": "string"
          },
          "category": {
            "type": "string"
          },
          "categoryId": {
            "type": "integer"
          },
          "description": {
            "type": "string"
          },
          "features": {
            "type": "object",
            "properties": {
              "centerOrigin": {
                "type": "boolean"
              },
              "usesSpeed": {
                "type": "boolean"
              },
              "usesPalette": {
                "type": "boolean"
              },
              "zoneAware": {
                "type": "boolean"
              },
              "dualStrip": {
                "type": "boolean"
              },
              "physicsBased": {
                "type": "boolean"
              },
              "audioReactive": {
                "type": "boolean"
              }
            }
          },
          "featureFlags": {
            "type": "integer",
            "description": "Raw feature flags byte for compact storage"
          }
        }
      },
      "Parameters": {
        "type": "object",
        "properties": {
          "brightness": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Global brightness (0-255)"
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50,
            "description": "Animation speed (1-50)"
          },
          "paletteId": {
            "type": "integer",
            "minimum": 0,
            "description": "Color palette ID"
          },
          "intensity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Effect intensity"
          },
          "saturation": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Color saturation"
          },
          "complexity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Visual complexity"
          },
          "variation": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Pattern variation"
          }
        }
      },
      "TransitionTypes": {
        "type": "object",
        "properties": {
          "types": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {
                  "type": "integer",
                  "description": "Transition type ID"
                },
                "name": {
                  "type": "string",
                  "description": "Transition name"
                },
                "description": {
                  "type": "string",
                  "description": "Transition description"
                },
                "centerOrigin": {
                  "type": "boolean",
                  "description": "Transition originates from center"
                }
              }
            }
          },
          "easingCurves": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {
                  "type": "integer",
                  "description": "Easing curve ID"
                },
                "name": {
                  "type": "string",
                  "description": "Easing curve name"
                }
              }
            }
          }
        }
      },
      "TransitionConfig": {
        "type": "object",
        "properties": {
          "enabled": {
            "type": "boolean",
            "description": "Random transitions enabled"
          },
          "defaultDuration": {
            "type": "integer",
            "description": "Default transition duration in ms"
          },
          "defaultType": {
            "type": "integer",
            "description": "Default transition type ID"
          },
          "defaultEasing": {
            "type": "integer",
            "description": "Default easing curve ID"
          }
        }
      },
      "BatchOperation": {
        "type": "object",
        "required": ["action"],
        "properties": {
          "action": {
            "type": "string",
            "enum": [
              "setBrightness",
              "setSpeed",
              "setEffect",
              "setPalette",
              "setZoneEffect",
              "setZoneBrightness",
              "setZoneSpeed"
            ],
            "description": "Operation type"
          },
          "value": {
            "type": "integer",
            "description": "Value for setBrightness or setSpeed"
          },
          "effectId": {
            "type": "integer",
            "description": "Effect ID for setEffect or setZoneEffect"
          },
          "paletteId": {
            "type": "integer",
            "description": "Palette ID for setPalette"
          },
          "zoneId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 3,
            "description": "Zone ID for zone operations"
          },
          "brightness": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Brightness for setZoneBrightness"
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50,
            "description": "Speed for setZoneSpeed"
          }
        }
      }
    },
    "responses": {
      "ValidationError": {
        "description": "Validation error (invalid JSON, missing field, out of range)",
        "content": {
          "application/json": {
            "schema": {
              "$ref": "#/components/schemas/ErrorResponse"
            },
            "examples": {
              "invalidJson": {
                "summary": "Invalid JSON",
                "value": {
                  "success": false,
                  "error": {
                    "code": "INVALID_JSON",
                    "message": "Malformed JSON"
                  },
                  "timestamp": 1234567890,
                  "version": "1.0"
                }
              },
              "missingField": {
                "summary": "Missing required field",
                "value": {
                  "success": false,
                  "error": {
                    "code": "MISSING_FIELD",
                    "message": "Effect ID required",
                    "field": "id"
                  },
                  "timestamp": 1234567890,
                  "version": "1.0"
                }
              },
              "outOfRange": {
                "summary": "Value out of range",
                "value": {
                  "success": false,
                  "error": {
                    "code": "OUT_OF_RANGE",
                    "message": "Speed must be 1-50",
                    "field": "speed"
                  },
                  "timestamp": 1234567890,
                  "version": "1.0"
                }
              }
            }
          }
        }
      },
      "RateLimitError": {
        "description": "Rate limit exceeded (20 requests/second)",
        "content": {
          "application/json": {
            "schema": {
              "$ref": "#/components/schemas/ErrorResponse"
            },
            "example": {
              "success": false,
              "error": {
                "code": "RATE_LIMITED",
                "message": "Too many requests, please slow down"
              },
              "timestamp": 1234567890,
              "version": "1.0"
            }
          }
        }
      },
      "NotFoundError": {
        "description": "Resource not found",
        "content": {
          "application/json": {
            "schema": {
              "$ref": "#/components/schemas/ErrorResponse"
            },
            "example": {
              "success": false,
              "error": {
                "code": "NOT_FOUND",
                "message": "Effect ID out of range",
                "field": "id"
              },
              "timestamp": 1234567890,
              "version": "1.0"
            }
          }
        }
      }
    }
  },
  "tags": [
    {
      "name": "Discovery",
      "description": "API discovery and capabilities"
    },
    {
      "name": "Device",
      "description": "Device status and information"
    },
    {
      "name": "Effects",
      "description": "Visual effects management"
    },
    {
      "name": "Parameters",
      "description": "Global visual parameters"
    },
    {
      "name": "Transitions",
      "description": "Effect transition system"
    },
    {
      "name": "Batch",
      "description": "Batch operations"
    }
  ],
  "externalDocs": {
    "description": "LightwaveOS GitHub Repository",
    "url": "https://github.com/example/lightwaveos"
  },
  "x-rateLimit": {
    "http": {
      "limit": 20,
      "window": "1s",
      "response": 429
    },
    "websocket": {
      "limit": 30,
      "window": "1s",
      "action": "error_message"
    }
  },
  "x-websocket": {
    "url": "ws://lightwaveos.local/ws",
    "description": "Real-time bidirectional communication for low-latency control and state updates. WebSocket messages follow the same success/error format as REST responses."
  }
})json";

#endif // OPENAPI_SPEC_H
