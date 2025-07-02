# Continuation Prompt: Web UI Migration from Mongoose to QNEthernet

## Session Summary

### Started With
- Successfully decoded Mongoose Wizard JSON format
- Created working EventLogger UI configuration
- Documented findings in MONGOOSE_WIZARD_GUIDE.md

### Major Pivot Discovery
Decided to migrate away from Mongoose completely because:

1. **OOM (Out of Memory) Issues**
   - Mongoose causes frequent memory problems
   - Required isolation in NetworkBase.h
   - Forced pointer workarounds throughout codebase

2. **Defensive Coding Tax**
   - Every class has memory management regimens
   - Throttling, careful allocations, limited buffers
   - Complexity spread throughout entire codebase

3. **Mongoose Provides Entire Network Stack**
   - Not just web server - complete TCP/IP implementation
   - DHCP client, DNS resolver, network management
   - Much bigger change than initially thought

### Key Documents Created

1. **WEB_UI_MIGRATION_PLAN.md** - Initial plan (web server only)
2. **WEB_UI_MIGRATION_PLAN_REVISED.md** - Complete plan including network stack
3. **MONGOOSE_WIZARD_GUIDE.md** - Documentation of Mongoose Wizard format

### Migration Strategy

Replace Mongoose with:
- **QNEthernet** - Network stack (already in project)
- **AsyncWebServer_Teensy41** - Web server
- **Custom HTML/CSS/JS** - Full control over UI

### Benefits of Migration

1. **Solve OOM issues** - QNEthernet uses predictable static buffers
2. **Remove complexity** - No more defensive coding patterns
3. **Better UI control** - No wizard limitations
4. **Native Teensy support** - Designed for the platform

### Current Status

Ready to begin Phase 0:
- Analyze all Mongoose dependencies
- Map defensive coding patterns
- Create abstraction layer for parallel testing

### Next Steps

1. Search codebase for all Mongoose usage:
   ```bash
   grep -r "mongoose.h"
   grep -r "mg_"
   grep -r "NetworkBase"
   ```

2. Identify defensive patterns to remove later

3. Begin implementing dual network stack for safe migration

### Important Context

- This is ~50 hours of work but will pay off in reduced complexity
- Each phase has a checkpoint for testing
- Can run both stacks in parallel for safety
- Git branch: feature/async-web-ui (to be created)

### Questions to Address

1. Should we start with analyzing NetworkBase.h?
2. What are the most critical Mongoose pain points to solve first?
3. Are there any specific defensive patterns you want to eliminate?

---

**Note**: The conversation pivoted from improving Mongoose Wizard UI to completely replacing Mongoose due to systemic memory issues. This is a fundamental architectural improvement, not just a UI enhancement.