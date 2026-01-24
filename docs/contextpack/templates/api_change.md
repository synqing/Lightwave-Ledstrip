# API Change Template

**Endpoint**: [Method] `/api/v1/[resource]/[action]`  
**Change Type**: [new|modify|deprecate|remove]

---

## Change Summary

**What Changed**:
- [Description of change]

**Why**:
- [Rationale]

---

## API Specification

### Request

**Method**: [GET|POST|PUT|DELETE|PATCH]  
**Path**: `/api/v1/[resource]/[action]`

**Headers**:
```
Content-Type: application/json
```

**Body** (if applicable):
```json
{
  "field1": "value1",
  "field2": 123
}
```

### Response

**Status Codes**:
- `200 OK`: [Description]
- `400 Bad Request`: [Description]
- `404 Not Found`: [Description]

**Response Body**:
```json
{
  "success": true,
  "data": { ... }
}
```

---

## Backward Compatibility

- [ ] Additive JSON only (new fields added, none removed)
- [ ] Old clients continue to work
- [ ] Deprecation notice added if breaking change required

---

## Implementation Checklist

### Firmware

- [ ] Handler in `firmware/v2/src/network/webserver/handlers/`
- [ ] OpenAPI spec updated in `WebServer.cpp`
- [ ] Request validation added
- [ ] Error handling implemented
- [ ] Thread safety verified (if shared state)

### Dashboard

- [ ] Client updated in `lightwave-dashboard/src/services/v2/api.ts`
- [ ] Types updated in `types.ts` if needed
- [ ] Error handling added
- [ ] UI updated if needed

### Documentation

- [ ] API docs updated in `docs/api/`
- [ ] Changelog entry added
- [ ] Breaking changes documented

---

## Acceptance Checks

- [ ] `GET /api/v1/effects` unchanged (backward compatibility)
- [ ] New endpoint returns expected response
- [ ] Dashboard client updated and tested
- [ ] No thread safety issues (WebSocket handlers)
