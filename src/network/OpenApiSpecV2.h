#ifndef OPENAPI_SPEC_V2_H
#define OPENAPI_SPEC_V2_H

/**
 * @file OpenApiSpecV2.h
 * @brief OpenAPI 3.0.3 specification for LightwaveOS API v2
 *
 * Provides machine-readable API documentation stored in PROGMEM.
 * Features RESTful resource-oriented design with proper HTTP verbs.
 *
 * Key v2 changes from v1:
 * - PUT for effect changes (idempotent)
 * - PATCH for partial parameter updates
 * - Zone resources with full CRUD operations
 * - Proper path parameters for resource IDs
 *
 * Access via: GET /api/v2/openapi.json
 */

#include <Arduino.h>

const char OPENAPI_SPEC_V2[] PROGMEM = R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "LightwaveOS API",
    "version": "2.0.0",
    "description": "ESP32-S3 LED control system for dual 160-LED WS2812 strips with 45+ visual effects, multi-zone composition, and CENTER ORIGIN design philosophy. API v2 provides RESTful resource-oriented endpoints with proper HTTP verb semantics.",
    "contact": {
      "name": "LightwaveOS",
      "url": "http://lightwaveos.local"
    },
    "license": {
      "name": "MIT"
    }
  },
  "servers": [
    {
      "url": "http://lightwaveos.local/api/v2",
      "description": "LightwaveOS Device (mDNS)"
    },
    {
      "url": "http://{deviceIP}/api/v2",
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
        "operationId": "getApiDiscovery",
        "summary": "API Discovery",
        "description": "Returns HATEOAS links, hardware information, and API v2 capabilities.",
        "tags": ["Discovery"],
        "responses": {
          "200": {
            "description": "API discovery information",
            "content": {
              "application/json": {
                "schema": {
                  "$ref": "#/components/schemas/ApiDiscovery"
                },
                "example": {
                  "success": true,
                  "data": {
                    "name": "LightwaveOS",
                    "apiVersion": "2.0",
                    "description": "ESP32-S3 LED Control System",
                    "hardware": {
                      "ledsTotal": 320,
                      "strips": 2,
                      "ledsPerStrip": 160,
                      "centerPoint": 79,
                      "maxZones": 4
                    },
                    "_links": {
                      "self": {"href": "/api/v2/"},
                      "openapi": {"href": "/api/v2/openapi.json"},
                      "device": {"href": "/api/v2/device"},
                      "effects": {"href": "/api/v2/effects"},
                      "parameters": {"href": "/api/v2/parameters"},
                      "transitions": {"href": "/api/v2/transitions"},
                      "zones": {"href": "/api/v2/zones"},
                      "batch": {"href": "/api/v2/batch"},
                      "websocket": {"href": "ws://lightwaveos.local/ws"}
                    }
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
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
    "/openapi.json": {
      "get": {
        "operationId": "getOpenApiSpec",
        "summary": "OpenAPI Specification",
        "description": "Returns this OpenAPI 3.0.3 specification document.",
        "tags": ["Discovery"],
        "responses": {
          "200": {
            "description": "OpenAPI specification",
            "content": {
              "application/json": {
                "schema": {
                  "type": "object",
                  "description": "OpenAPI 3.0.3 specification document"
                }
              }
            }
          }
        }
      }
    },
    "/device": {
      "get": {
        "operationId": "getDevice",
        "summary": "Device Overview",
        "description": "Returns combined device status and information.",
        "tags": ["Device"],
        "responses": {
          "200": {
            "description": "Device overview",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "allOf": [
                            {"$ref": "#/components/schemas/DeviceStatus"},
                            {"$ref": "#/components/schemas/DeviceInfo"}
                          ]
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
    "/device/status": {
      "get": {
        "operationId": "getDeviceStatus",
        "summary": "Device Status",
        "description": "Returns runtime status including uptime, heap memory, network status, and WebSocket client count.",
        "tags": ["Device"],
        "responses": {
          "200": {
            "description": "Current device status",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/DeviceStatus"}
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
        "operationId": "getDeviceInfo",
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
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/DeviceInfo"}
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
        "operationId": "listEffects",
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
            "description": "Number of effects to return (max 50)",
            "schema": {
              "type": "integer",
              "minimum": 1,
              "maximum": 50,
              "default": 20
            }
          },
          {
            "name": "category",
            "in": "query",
            "description": "Filter by category ID",
            "schema": {
              "type": "integer",
              "minimum": 0
            }
          },
          {
            "name": "details",
            "in": "query",
            "description": "Include detailed metadata (features, descriptions)",
            "schema": {
              "type": "boolean",
              "default": false
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
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/EffectsList"}
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
        "operationId": "getCurrentEffect",
        "summary": "Get Current Effect",
        "description": "Returns currently active effect and its parameters.",
        "tags": ["Effects"],
        "responses": {
          "200": {
            "description": "Current effect information",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/CurrentEffect"}
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
      "put": {
        "operationId": "setCurrentEffect",
        "summary": "Set Current Effect",
        "description": "Activates a specific effect by ID. This is an idempotent operation.",
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
                    "maximum": 255,
                    "description": "Effect ID to activate"
                  },
                  "transition": {
                    "type": "boolean",
                    "default": true,
                    "description": "Use transition animation"
                  }
                }
              },
              "example": {
                "effectId": 5,
                "transition": true
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
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "effectId": {"type": "integer"},
                            "name": {"type": "string"},
                            "transitioned": {"type": "boolean"}
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
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/effects/{id}": {
      "get": {
        "operationId": "getEffect",
        "summary": "Get Effect Details",
        "description": "Returns detailed metadata for a specific effect including category, features, and description.",
        "tags": ["Effects"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Effect ID",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 255
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
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/EffectMetadata"}
                      }
                    }
                  ]
                }
              }
            }
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
    "/effects/categories": {
      "get": {
        "operationId": "listEffectCategories",
        "summary": "List Effect Categories",
        "description": "Returns all effect categories with their IDs and names.",
        "tags": ["Effects"],
        "responses": {
          "200": {
            "description": "List of categories",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "categories": {
                              "type": "array",
                              "items": {"$ref": "#/components/schemas/Category"}
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
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/parameters": {
      "get": {
        "operationId": "getParameters",
        "summary": "Get All Parameters",
        "description": "Returns all visual parameters (brightness, speed, palette, intensity, etc.).",
        "tags": ["Parameters"],
        "responses": {
          "200": {
            "description": "Current parameter values",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/Parameters"}
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
      "patch": {
        "operationId": "updateParameters",
        "summary": "Update Parameters",
        "description": "Partially updates one or more visual parameters. Only include fields you want to change.",
        "tags": ["Parameters"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/Parameters"},
              "example": {
                "brightness": 200,
                "speed": 25
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
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/Parameters"}
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
    "/parameters/{name}": {
      "get": {
        "operationId": "getParameter",
        "summary": "Get Single Parameter",
        "description": "Returns the value of a specific parameter.",
        "tags": ["Parameters"],
        "parameters": [
          {
            "name": "name",
            "in": "path",
            "required": true,
            "description": "Parameter name",
            "schema": {
              "type": "string",
              "enum": ["brightness", "speed", "paletteId", "intensity", "saturation", "complexity", "variation"]
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Parameter value",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "name": {"type": "string"},
                            "value": {"type": "integer"},
                            "min": {"type": "integer"},
                            "max": {"type": "integer"}
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "put": {
        "operationId": "setParameter",
        "summary": "Set Single Parameter",
        "description": "Sets the value of a specific parameter.",
        "tags": ["Parameters"],
        "parameters": [
          {
            "name": "name",
            "in": "path",
            "required": true,
            "description": "Parameter name",
            "schema": {
              "type": "string",
              "enum": ["brightness", "speed", "paletteId", "intensity", "saturation", "complexity", "variation"]
            }
          }
        ],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "required": ["value"],
                "properties": {
                  "value": {
                    "type": "integer",
                    "minimum": 0,
                    "maximum": 255,
                    "description": "Parameter value"
                  }
                }
              },
              "example": {
                "value": 200
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Parameter updated",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "name": {"type": "string"},
                            "value": {"type": "integer"}
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
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/transitions": {
      "get": {
        "operationId": "listTransitions",
        "summary": "List Transition Types",
        "description": "Returns list of available transition types and easing curves.",
        "tags": ["Transitions"],
        "responses": {
          "200": {
            "description": "Transition types and easing curves",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/TransitionTypes"}
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
        "operationId": "getTransitionConfig",
        "summary": "Get Transition Configuration",
        "description": "Returns current transition settings.",
        "tags": ["Transitions"],
        "responses": {
          "200": {
            "description": "Transition configuration",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/TransitionConfig"}
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
      "patch": {
        "operationId": "updateTransitionConfig",
        "summary": "Update Transition Configuration",
        "description": "Partially updates transition settings.",
        "tags": ["Transitions"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/TransitionConfig"},
              "example": {
                "enabled": true,
                "defaultDuration": 1500
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Configuration updated",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/TransitionConfig"}
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
    "/transitions/trigger": {
      "post": {
        "operationId": "triggerTransition",
        "summary": "Trigger Transition",
        "description": "Manually triggers a transition to a specific effect with optional type, duration, and easing.",
        "tags": ["Transitions"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/TransitionTrigger"},
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
            "description": "Transition triggered",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "effectId": {"type": "integer"},
                            "name": {"type": "string"},
                            "transitionType": {"type": "integer"},
                            "duration": {"type": "integer"},
                            "easing": {"type": "integer"}
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
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/zones": {
      "get": {
        "operationId": "listZones",
        "summary": "List All Zones",
        "description": "Returns all zone configurations including effect, parameters, and enabled state.",
        "tags": ["Zones"],
        "responses": {
          "200": {
            "description": "List of zones",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "enabled": {
                              "type": "boolean",
                              "description": "Zone system enabled"
                            },
                            "zoneCount": {
                              "type": "integer",
                              "minimum": 1,
                              "maximum": 4,
                              "description": "Number of active zones"
                            },
                            "zones": {
                              "type": "array",
                              "items": {"$ref": "#/components/schemas/Zone"}
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
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "post": {
        "operationId": "configureZones",
        "summary": "Configure Zone System",
        "description": "Enables/disables zone system and sets zone count.",
        "tags": ["Zones"],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {
                "type": "object",
                "properties": {
                  "enabled": {
                    "type": "boolean",
                    "description": "Enable zone system"
                  },
                  "zoneCount": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 4,
                    "description": "Number of zones (1-4)"
                  }
                }
              },
              "example": {
                "enabled": true,
                "zoneCount": 4
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Zone system configured",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "enabled": {"type": "boolean"},
                            "zoneCount": {"type": "integer"}
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
    "/zones/{id}": {
      "get": {
        "operationId": "getZone",
        "summary": "Get Zone",
        "description": "Returns configuration for a specific zone.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Zone configuration",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/Zone"}
                      }
                    }
                  ]
                }
              }
            }
          },
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "patch": {
        "operationId": "updateZone",
        "summary": "Update Zone",
        "description": "Partially updates zone configuration.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/ZoneUpdate"},
              "example": {
                "effectId": 5,
                "brightness": 200,
                "enabled": true
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Zone updated",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/Zone"}
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
      },
      "delete": {
        "operationId": "resetZone",
        "summary": "Reset Zone",
        "description": "Resets zone to default configuration (disables and clears effect).",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Zone reset",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "zoneId": {"type": "integer"},
                            "reset": {"type": "boolean", "enum": [true]}
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
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
    "/zones/{id}/effect": {
      "get": {
        "operationId": "getZoneEffect",
        "summary": "Get Zone Effect",
        "description": "Returns the current effect for a specific zone.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Zone effect",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "zoneId": {"type": "integer"},
                            "effectId": {"type": "integer"},
                            "name": {"type": "string"}
                          }
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "put": {
        "operationId": "setZoneEffect",
        "summary": "Set Zone Effect",
        "description": "Sets the effect for a specific zone.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
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
                    "maximum": 255,
                    "description": "Effect ID"
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
            "description": "Zone effect set",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "zoneId": {"type": "integer"},
                            "effectId": {"type": "integer"},
                            "name": {"type": "string"}
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
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/zones/{id}/parameters": {
      "get": {
        "operationId": "getZoneParameters",
        "summary": "Get Zone Parameters",
        "description": "Returns all parameters for a specific zone.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "responses": {
          "200": {
            "description": "Zone parameters",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "allOf": [
                            {
                              "type": "object",
                              "properties": {
                                "zoneId": {"type": "integer"}
                              }
                            },
                            {"$ref": "#/components/schemas/ZoneParameters"}
                          ]
                        }
                      }
                    }
                  ]
                }
              }
            }
          },
          "404": {
            "$ref": "#/components/responses/NotFoundError"
          },
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      },
      "patch": {
        "operationId": "updateZoneParameters",
        "summary": "Update Zone Parameters",
        "description": "Partially updates parameters for a specific zone.",
        "tags": ["Zones"],
        "parameters": [
          {
            "name": "id",
            "in": "path",
            "required": true,
            "description": "Zone ID (0-3)",
            "schema": {
              "type": "integer",
              "minimum": 0,
              "maximum": 3
            }
          }
        ],
        "requestBody": {
          "required": true,
          "content": {
            "application/json": {
              "schema": {"$ref": "#/components/schemas/ZoneParameters"},
              "example": {
                "brightness": 200,
                "speed": 25,
                "intensity": 180
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Zone parameters updated",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "allOf": [
                            {
                              "type": "object",
                              "properties": {
                                "zoneId": {"type": "integer"}
                              }
                            },
                            {"$ref": "#/components/schemas/ZoneParameters"}
                          ]
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
    "/zones/presets": {
      "get": {
        "operationId": "listZonePresets",
        "summary": "List Zone Presets",
        "description": "Returns available zone presets (built-in and user-defined).",
        "tags": ["Zones"],
        "responses": {
          "200": {
            "description": "Zone presets",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {
                          "type": "object",
                          "properties": {
                            "builtIn": {
                              "type": "array",
                              "items": {"$ref": "#/components/schemas/ZonePreset"}
                            },
                            "user": {
                              "type": "array",
                              "items": {"$ref": "#/components/schemas/ZonePreset"}
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
          "429": {
            "$ref": "#/components/responses/RateLimitError"
          }
        }
      }
    },
    "/batch": {
      "post": {
        "operationId": "executeBatch",
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
                    "items": {"$ref": "#/components/schemas/BatchOperation"}
                  }
                }
              },
              "example": {
                "operations": [
                  {"action": "setBrightness", "value": 200},
                  {"action": "setEffect", "effectId": 5},
                  {"action": "setSpeed", "value": 30}
                ]
              }
            }
          }
        },
        "responses": {
          "200": {
            "description": "Batch results",
            "content": {
              "application/json": {
                "schema": {
                  "allOf": [
                    {"$ref": "#/components/schemas/SuccessResponse"},
                    {
                      "properties": {
                        "data": {"$ref": "#/components/schemas/BatchResult"}
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
            "description": "Server timestamp in milliseconds since boot"
          },
          "version": {
            "type": "string",
            "enum": ["2.0"],
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
                  "NOT_FOUND",
                  "METHOD_NOT_ALLOWED",
                  "INTERNAL_ERROR"
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
            "enum": ["2.0"],
            "description": "API version"
          }
        }
      },
      "ApiDiscovery": {
        "allOf": [
          {"$ref": "#/components/schemas/SuccessResponse"},
          {
            "properties": {
              "data": {
                "type": "object",
                "properties": {
                  "name": {"type": "string"},
                  "apiVersion": {"type": "string"},
                  "description": {"type": "string"},
                  "hardware": {
                    "type": "object",
                    "properties": {
                      "ledsTotal": {"type": "integer"},
                      "strips": {"type": "integer"},
                      "ledsPerStrip": {"type": "integer"},
                      "centerPoint": {"type": "integer"},
                      "maxZones": {"type": "integer"}
                    }
                  },
                  "_links": {
                    "type": "object",
                    "additionalProperties": {
                      "type": "object",
                      "properties": {
                        "href": {"type": "string"}
                      }
                    }
                  }
                }
              }
            }
          }
        ]
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
          "minFreeHeap": {
            "type": "integer",
            "description": "Minimum free heap since boot"
          },
          "cpuFreq": {
            "type": "integer",
            "description": "CPU frequency in MHz"
          },
          "temperature": {
            "type": "number",
            "description": "CPU temperature in Celsius (if available)"
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
                "description": "IP address"
              },
              "hostname": {
                "type": "string",
                "description": "mDNS hostname"
              },
              "rssi": {
                "type": "integer",
                "description": "WiFi signal strength in dBm"
              },
              "ssid": {
                "type": "string",
                "description": "Connected network SSID"
              }
            }
          },
          "wsClients": {
            "type": "integer",
            "description": "Number of active WebSocket clients"
          },
          "fps": {
            "type": "number",
            "description": "Current render FPS"
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
          "chip": {
            "type": "string",
            "description": "Chip model (e.g., ESP32-S3)"
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
          },
          "features": {
            "type": "array",
            "items": {"type": "string"},
            "description": "Enabled feature flags"
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
            "items": {"$ref": "#/components/schemas/Effect"}
          },
          "_links": {
            "type": "object",
            "properties": {
              "next": {"type": "string"},
              "prev": {"type": "string"}
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
          "category": {
            "type": "string",
            "description": "Effect category"
          },
          "transitioning": {
            "type": "boolean",
            "description": "Currently in transition"
          },
          "parameters": {
            "$ref": "#/components/schemas/Parameters"
          }
        }
      },
      "EffectMetadata": {
        "type": "object",
        "properties": {
          "id": {
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
              "centerOrigin": {"type": "boolean"},
              "usesSpeed": {"type": "boolean"},
              "usesPalette": {"type": "boolean"},
              "zoneAware": {"type": "boolean"},
              "dualStrip": {"type": "boolean"},
              "physicsBased": {"type": "boolean"},
              "audioReactive": {"type": "boolean"}
            }
          },
          "featureFlags": {
            "type": "integer",
            "description": "Raw feature flags byte for compact storage"
          }
        }
      },
      "Category": {
        "type": "object",
        "properties": {
          "id": {
            "type": "integer"
          },
          "name": {
            "type": "string"
          },
          "count": {
            "type": "integer",
            "description": "Number of effects in this category"
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
            "maximum": 255,
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
                "id": {"type": "integer"},
                "name": {"type": "string"},
                "description": {"type": "string"},
                "centerOrigin": {"type": "boolean"}
              }
            }
          },
          "easingCurves": {
            "type": "array",
            "items": {
              "type": "object",
              "properties": {
                "id": {"type": "integer"},
                "name": {"type": "string"}
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
            "description": "Auto-transitions enabled"
          },
          "defaultDuration": {
            "type": "integer",
            "minimum": 100,
            "maximum": 5000,
            "description": "Default transition duration in ms"
          },
          "defaultType": {
            "type": "integer",
            "minimum": 0,
            "maximum": 11,
            "description": "Default transition type ID"
          },
          "defaultEasing": {
            "type": "integer",
            "minimum": 0,
            "maximum": 14,
            "description": "Default easing curve ID"
          }
        }
      },
      "TransitionTrigger": {
        "type": "object",
        "required": ["toEffect"],
        "properties": {
          "toEffect": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
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
      "Zone": {
        "type": "object",
        "properties": {
          "id": {
            "type": "integer",
            "minimum": 0,
            "maximum": 3,
            "description": "Zone ID"
          },
          "enabled": {
            "type": "boolean",
            "description": "Zone enabled"
          },
          "effectId": {
            "type": "integer",
            "description": "Current effect ID"
          },
          "effectName": {
            "type": "string",
            "description": "Current effect name"
          },
          "parameters": {
            "$ref": "#/components/schemas/ZoneParameters"
          },
          "ledRange": {
            "type": "object",
            "properties": {
              "start": {"type": "integer"},
              "end": {"type": "integer"},
              "count": {"type": "integer"}
            }
          },
          "blendMode": {
            "type": "string",
            "enum": ["replace", "blend", "add", "subtract", "multiply"],
            "description": "Blend mode for zone composition"
          }
        }
      },
      "ZoneUpdate": {
        "type": "object",
        "properties": {
          "enabled": {
            "type": "boolean"
          },
          "effectId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255
          },
          "brightness": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50
          },
          "paletteId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255
          },
          "blendMode": {
            "type": "string",
            "enum": ["replace", "blend", "add", "subtract", "multiply"]
          }
        }
      },
      "ZoneParameters": {
        "type": "object",
        "properties": {
          "brightness": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone brightness (0-255)"
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50,
            "description": "Zone animation speed (1-50)"
          },
          "paletteId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone palette ID (0=global)"
          },
          "intensity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone effect intensity"
          },
          "saturation": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone color saturation"
          },
          "complexity": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone visual complexity"
          },
          "variation": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Zone pattern variation"
          }
        }
      },
      "ZonePreset": {
        "type": "object",
        "properties": {
          "id": {
            "type": "integer",
            "description": "Preset ID"
          },
          "name": {
            "type": "string",
            "description": "Preset name"
          },
          "zoneCount": {
            "type": "integer",
            "description": "Number of zones in preset"
          },
          "builtIn": {
            "type": "boolean",
            "description": "True for system presets"
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
              "setIntensity",
              "setSaturation",
              "setComplexity",
              "setVariation",
              "setZoneEffect",
              "setZoneBrightness",
              "setZoneSpeed",
              "setZonePalette",
              "triggerTransition"
            ],
            "description": "Operation type"
          },
          "value": {
            "type": "integer",
            "description": "Value for single-value operations"
          },
          "effectId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Effect ID for effect operations"
          },
          "paletteId": {
            "type": "integer",
            "minimum": 0,
            "maximum": 255,
            "description": "Palette ID for palette operations"
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
            "maximum": 255
          },
          "speed": {
            "type": "integer",
            "minimum": 1,
            "maximum": 50
          },
          "transitionType": {
            "type": "integer",
            "minimum": 0,
            "maximum": 11
          },
          "duration": {
            "type": "integer",
            "minimum": 100,
            "maximum": 5000
          }
        }
      },
      "BatchResult": {
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
                "index": {"type": "integer"},
                "action": {"type": "string"},
                "success": {"type": "boolean"},
                "error": {"type": "string"}
              }
            }
          }
        }
      }
    },
    "responses": {
      "ValidationError": {
        "description": "Validation error (invalid JSON, missing field, out of range)",
        "content": {
          "application/json": {
            "schema": {"$ref": "#/components/schemas/ErrorResponse"},
            "examples": {
              "invalidJson": {
                "summary": "Invalid JSON",
                "value": {
                  "success": false,
                  "error": {
                    "code": "INVALID_JSON",
                    "message": "Malformed JSON in request body"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
                }
              },
              "missingField": {
                "summary": "Missing required field",
                "value": {
                  "success": false,
                  "error": {
                    "code": "MISSING_FIELD",
                    "message": "Required field 'effectId' is missing",
                    "field": "effectId"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
                }
              },
              "outOfRange": {
                "summary": "Value out of range",
                "value": {
                  "success": false,
                  "error": {
                    "code": "OUT_OF_RANGE",
                    "message": "Speed must be between 1 and 50",
                    "field": "speed"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
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
            "schema": {"$ref": "#/components/schemas/ErrorResponse"},
            "example": {
              "success": false,
              "error": {
                "code": "RATE_LIMITED",
                "message": "Too many requests, please slow down"
              },
              "timestamp": 1234567890,
              "version": "2.0"
            }
          }
        }
      },
      "NotFoundError": {
        "description": "Resource not found",
        "content": {
          "application/json": {
            "schema": {"$ref": "#/components/schemas/ErrorResponse"},
            "examples": {
              "effectNotFound": {
                "summary": "Effect not found",
                "value": {
                  "success": false,
                  "error": {
                    "code": "NOT_FOUND",
                    "message": "Effect ID 99 not found",
                    "field": "id"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
                }
              },
              "zoneNotFound": {
                "summary": "Zone not found",
                "value": {
                  "success": false,
                  "error": {
                    "code": "NOT_FOUND",
                    "message": "Zone ID 5 does not exist (valid: 0-3)",
                    "field": "id"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
                }
              },
              "parameterNotFound": {
                "summary": "Parameter not found",
                "value": {
                  "success": false,
                  "error": {
                    "code": "NOT_FOUND",
                    "message": "Unknown parameter 'foo'",
                    "field": "name"
                  },
                  "timestamp": 1234567890,
                  "version": "2.0"
                }
              }
            }
          }
        }
      }
    }
  },
  "tags": [
    {
      "name": "Discovery",
      "description": "API discovery and OpenAPI specification"
    },
    {
      "name": "Device",
      "description": "Device status and hardware information"
    },
    {
      "name": "Effects",
      "description": "Visual effects management and metadata"
    },
    {
      "name": "Parameters",
      "description": "Global visual parameters (brightness, speed, palette, etc.)"
    },
    {
      "name": "Transitions",
      "description": "Effect transition system with 12 types and 15 easing curves"
    },
    {
      "name": "Zones",
      "description": "Multi-zone composition with up to 4 independent zones"
    },
    {
      "name": "Batch",
      "description": "Atomic batch operations (max 10 per request)"
    }
  ],
  "externalDocs": {
    "description": "LightwaveOS Documentation",
    "url": "https://github.com/example/lightwaveos"
  },
  "x-rateLimit": {
    "http": {
      "limit": 20,
      "window": "1s",
      "response": 429
    },
    "websocket": {
      "limit": 50,
      "window": "1s",
      "action": "error_message"
    }
  },
  "x-websocket": {
    "url": "ws://lightwaveos.local/ws",
    "description": "Real-time bidirectional communication. WebSocket messages follow the same success/error format as REST responses.",
    "commands": [
      "effects.getCurrent",
      "effects.getMetadata",
      "effects.getCategories",
      "parameters.get",
      "device.getStatus",
      "transition.trigger",
      "transition.getTypes",
      "transition.config",
      "zones.get",
      "batch"
    ]
  },
  "x-hardware": {
    "platform": "ESP32-S3-DevKitC-1",
    "cpu": "240MHz dual-core Xtensa LX7",
    "flash": "8MB",
    "leds": {
      "total": 320,
      "strips": 2,
      "perStrip": 160,
      "type": "WS2812",
      "gpio": [4, 5]
    },
    "centerOrigin": {
      "description": "All effects must originate from LED 79/80 (center point)",
      "centerLed": 79,
      "constraint": "Effects propagate OUTWARD from center or INWARD from edges"
    }
  }
})json";

#endif // OPENAPI_SPEC_V2_H
