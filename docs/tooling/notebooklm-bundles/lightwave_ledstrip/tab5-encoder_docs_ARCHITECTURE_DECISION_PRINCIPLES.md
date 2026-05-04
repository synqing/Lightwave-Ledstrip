---
abstract: "Maps the 12 UI design decision principles from DESIGN_DECISION_PROCESS.md to their software architecture equivalents. Each principle includes a concrete before/after example, the anti-pattern it prevents, and application to database selection, monolith-vs-microservices, API versioning, and the tab5-encoder main.cpp decomposition."
---

# Architecture Decision Principles

The 12 principles discovered during the Tab5 UI parameter layout exercise (documented in `DESIGN_DECISION_PROCESS.md`) are not UI-specific. They are **decision-making principles** that apply identically to software architecture. This document translates each one into its architecture equivalent with concrete examples.

---

## Principle 1: Diagnose the Disease, Not the Symptom

### UI Version
"A cluttered UI might be visual hierarchy OR information architecture. The fix is different."

### Architecture Equivalent
**An architecture problem masquerading as a performance problem will not be solved by performance tuning.** When someone says "the API is slow," there are at least four distinct diseases: an N+1 query pattern (data access architecture), a missing cache layer (infrastructure architecture), synchronous I/O where async is needed (concurrency architecture), or an over-fetched response payload (API design). Each requires a fundamentally different fix. Applying the wrong one wastes the entire effort.

### Before/After Example

**Before (treating the symptom):** A REST endpoint returns a user profile with their 50 most recent orders, each with line items and product details. Response time: 2.3 seconds. The team adds Redis caching on the endpoint. Cache hit: fast. Cache miss: still 2.3 seconds. Cache invalidation becomes a new bug surface. The disease was an N+1 query pattern -- one query for the user, one per order, one per line item, one per product. 153 database round-trips per request.

**After (treating the disease):** The team profiles the endpoint and finds 153 SQL queries per request. They restructure to 3 queries with JOINs, or introduce a read-model projection that pre-computes the response shape. Response time: 45ms. No cache needed. The cache was treating the symptom (slow response). The JOIN restructure treated the disease (N+1 data access pattern).

### Anti-Pattern Prevented
**Cache-as-band-aid.** Adding caching to hide architectural inefficiency. The cache becomes load-bearing infrastructure that masks a design flaw, adds invalidation complexity, and creates consistency bugs -- all while the underlying disease persists for every cache miss.

---

## Principle 2: Research the Right Domain

### UI Version
"'How do 16 encoders manage 50+ params' not 'what colours look good on dark backgrounds.'"

### Architecture Equivalent
**Research the operational domain, not the technology domain.** When choosing a message broker, the right question is not "Kafka vs RabbitMQ features" but "what are our actual message delivery guarantees, throughput, and ordering requirements?" Technology comparison without domain requirements is shopping without a list.

### Before/After Example

**Before (wrong domain):** A team spends two weeks evaluating Kafka, RabbitMQ, NATS, and Redis Streams. They produce a 40-page comparison matrix covering throughput benchmarks, clustering modes, and protocol differences. They choose Kafka because it has the highest throughput. Their actual workload: 200 events/minute from a webhook ingestion service with at-least-once delivery needs. They now maintain ZooKeeper (or KRaft), deal with partition rebalancing, and fight consumer group lag for a workload that Redis Streams would handle trivially.

**After (right domain):** The team starts with their requirements: 200 events/minute peak, at-least-once delivery, 3 consumers, no strict ordering needed, team of 2 with no Kafka experience. They evaluate options against these constraints. Redis Streams wins: zero new infrastructure (Redis already deployed), trivial consumer groups, and the team can operate it without a dedicated platform engineer.

### Anti-Pattern Prevented
**Resume-Driven Architecture.** Choosing technology because it is impressive rather than because it fits the problem domain. The research effort was thorough but aimed at the wrong target.

---

## Principle 3: Propose Structure Before Visuals, but Render Structure Visually

### UI Version
"Architecture in words, evaluation in pixels."

### Architecture Equivalent
**Define the system boundaries in domain terms first, then render them as diagrams for evaluation. Never skip the diagram.** A written description of service boundaries ("the order service handles checkout and the payment service handles billing") is ambiguous. A C4 container diagram showing the exact API contracts, data flows, and synchronous vs asynchronous boundaries is evaluable. Stakeholders who approve a text description are approving their own mental model, not yours.

### Before/After Example

**Before (structure without rendering):** An ADR describes a microservices split: "We will extract the notification subsystem into its own service." The team agrees. During implementation, they discover the notification service needs synchronous access to user preferences (owned by the user service), subscription tier (owned by the billing service), and delivery status (owned by itself). Nobody visualised these dependencies. The "independent" notification service is now coupled to two others via synchronous REST calls, and any outage in user-service silences all notifications.

**After (structure rendered visually):** Before approving the ADR, the team draws a container diagram showing every inter-service call. The three synchronous dependencies are immediately visible. The team decides to push user preferences and subscription tier into the notification service's own read model via domain events, eliminating runtime coupling. The diagram took 30 minutes. The runtime coupling would have taken weeks to untangle after deployment.

### Anti-Pattern Prevented
**Whiteboard amnesia.** Agreeing on architecture through verbal or written descriptions that each stakeholder interprets differently. The structure exists in words but has never been stress-tested visually, so hidden coupling, circular dependencies, and missing contracts go undetected until implementation.

---

## Principle 4: Audit What Works Before Redesigning

### UI Version
"Not everything in a 'bad' design is bad. Surgically identify what works, what's neutral, what fails."

### Architecture Equivalent
**Before any migration or rewrite, inventory what the current system does well and protect it.** A monolith that "doesn't scale" might have excellent transactional consistency, a well-tested domain model, and zero distributed system failure modes. Extracting microservices solves the scaling problem but destroys these properties unless you deliberately preserve them.

### Before/After Example

**Before (nuke everything):** A team rewrites their Django monolith as microservices because "the monolith is slow." The monolith had 94% test coverage, atomic database transactions across the entire domain, sub-second deploys, and a single Postgres instance that was easy to reason about. The microservices rewrite introduces eventual consistency bugs, distributed tracing complexity, 12 separate deployment pipelines, and test coverage drops to 61% because integration testing across services is hard. Deployment time goes from 30 seconds to 15 minutes. The "slow" part (one expensive report query) could have been solved with a read replica.

**After (surgical fix):** The team profiles the monolith and finds that 3 of 47 endpoints account for 80% of latency. They extract those 3 into a single read-optimised service backed by a materialised view. The rest of the monolith stays intact with its 94% coverage, atomic transactions, and simple operations. Total effort: 2 weeks. Total risk: low. The properties that worked (consistency, testability, operational simplicity) were preserved.

### Anti-Pattern Prevented
**Second-system syndrome.** The belief that a rewrite will be better because "now we know what we're doing." In practice, the rewrite loses the accumulated bug fixes, edge case handling, and implicit knowledge embedded in the old system.

---

## Principle 5: Hierarchy Requires 3+ Levels with Clear Differentiation

### UI Version
"Frame / content / active -- each visually distinct."

### Architecture Equivalent
**Layered architecture requires at least three distinct levels with enforced boundaries: presentation, domain, and infrastructure.** When layers bleed into each other -- SQL queries in controllers, HTTP response codes in domain logic, business rules in database triggers -- the architecture has no hierarchy. Everything talks to everything. Changes propagate everywhere.

### Before/After Example

**Before (flat hierarchy):** A Spring Boot controller method fetches data with an inline JPQL query, applies a discount calculation, formats the response as JSON, and sends an email notification -- all in one 80-line method. The presentation layer (JSON formatting), domain layer (discount logic), and infrastructure layer (database query, email sending) are fused. Changing the discount formula requires modifying a controller. Changing the email provider requires touching business logic. Testing the discount formula requires a running database and an email mock.

**After (three distinct levels):**
- **Presentation (controller):** Accepts HTTP request, calls domain service, returns DTO. No business logic, no infrastructure calls.
- **Domain (service + entity):** Calculates discount, emits a `DiscountApplied` domain event. No HTTP concepts, no database concepts, no email concepts.
- **Infrastructure (repository + event handler):** Persists data, handles the domain event by sending the email. No business rules.

Changing the discount formula touches only the domain layer. Changing the email provider touches only the infrastructure layer. The domain layer is testable with zero infrastructure. Each layer is visually distinct in the codebase by package/directory -- you can tell which layer you are in by the import statements.

### Anti-Pattern Prevented
**Layer bleed / Smart UI.** Domain logic migrating into controllers or views because "it's faster to just do it here." Once one business rule lives in the presentation layer, the boundary is broken and more follow.

---

## Principle 6: Placement Decisions Have Sub-Decisions -- Test Each Level

### UI Version
"Don't stop at 'sidebar.' Ask 'inside or outside the frame?'"

### Architecture Equivalent
**Every architectural placement decision has nested decisions that must be evaluated separately.** Deciding "we need a cache" is level 1. Level 2: where in the stack? (Application layer? Database query cache? CDN? Reverse proxy?) Level 3: what eviction policy? (TTL? LRU? Write-through? Write-behind?) Each sub-decision changes the system's behaviour in ways that the top-level decision does not capture.

### Before/After Example

**Before (stopped at level 1):** The team decides "we need caching for product listings." They add Redis between the API and the database. Products update every 30 minutes via a CMS. The cache TTL is set to 5 minutes. Result: 5-minute stale data, cache stampede on expiry, and the Redis instance becomes a single point of failure. Nobody asked the sub-questions.

**After (tested each level):**
- Level 1: "We need caching." Yes.
- Level 2: "Where?" Products change every 30 minutes and are read-heavy. CDN edge cache with `Cache-Control: max-age=1800, stale-while-revalidate=60` handles this without application-layer caching at all.
- Level 3: "Invalidation?" CMS publishes a webhook on product update. The API sends a CDN purge for the affected product URL.
- Result: no Redis, no cache stampede, sub-100ms globally, instant invalidation on update.

### Anti-Pattern Prevented
**Architecture by noun.** "We need a cache" / "We need a queue" / "We need a service mesh" -- stopping at the technology noun without drilling into the placement, configuration, and operational sub-decisions that determine whether it actually helps.

---

## Principle 7: Generate More Options Than You Think You Need

### UI Version
"The cost of generating an extra mockup is 5 minutes. The cost of committing to the wrong aesthetic is hours."

### Architecture Equivalent
**For irreversible architectural decisions, generate and evaluate at least 3 options before committing.** The cost of writing an additional ADR option section is an hour. The cost of discovering that your chosen database cannot handle your access pattern after 6 months of development is a migration project. Irreversible decisions deserve exhaustive option generation.

### Before/After Example

**Before (two options, premature commitment):** "Should we use PostgreSQL or MongoDB?" The team picks MongoDB because "our data is JSON-like." Six months later, they discover they need cross-collection transactions for their checkout flow, complex aggregation queries that MongoDB handles poorly, and that "JSON-like" data actually has strong relational invariants (orders reference products reference categories). Migration cost: 3 months.

**After (five options, constraint-driven):**
- Option A: PostgreSQL with JSONB columns for flexible fields
- Option B: MongoDB with denormalised documents
- Option C: PostgreSQL for transactional data + MongoDB for catalogue/search
- Option D: SQLite for the prototype, migrate to PostgreSQL at scale
- Option E: CockroachDB for distributed SQL from day one

Each is evaluated against: transaction requirements (strong -- kills pure MongoDB), query complexity (high -- favours SQL), team expertise (PostgreSQL), scale timeline (18 months to first scaling concern), and operational budget (1 engineer). Option A wins on all axes. The team avoids a costly migration.

### Anti-Pattern Prevented
**Binary framing.** Presenting an architectural choice as "A or B?" when C, D, and E also exist. Binary framing creates artificial urgency and hides options that may be superior to both.

---

## Principle 8: Every Property Must Carry Information

### UI Version
"If a colour doesn't tell the user something, it's decoration competing with information."

### Architecture Equivalent
**Every architectural element must justify its existence with a specific responsibility. If a service, layer, or abstraction does not carry a distinct concern, it is structural decoration -- overhead that competes with real responsibilities for attention and maintenance budget.**

### Before/After Example

**Before (decorative architecture):** A team creates a "clean architecture" with 7 layers: Controller -> RequestValidator -> UseCase -> DomainService -> Repository -> DataMapper -> Entity. The RequestValidator just calls `javax.validation` annotations that are already on the DTO. The DataMapper copies fields 1:1 between the entity and a database row object that is identical to the entity. The DomainService delegates to the Repository without adding any logic. Three of seven layers carry no information -- they exist because the architecture diagram says they should.

**After (every element carries information):** Controller (HTTP translation), UseCase (orchestration + authorisation), Entity (business rules + invariants), Repository (persistence). Four layers, each with a distinct responsibility. The eliminated layers were structural decoration: they existed to satisfy a pattern, not to solve a problem. The codebase is 40% smaller with zero loss of capability.

### Anti-Pattern Prevented
**Lasagne architecture.** Too many layers, each too thin to carry meaningful responsibility. The layers exist for "separation of concerns" but separate nothing -- they just add indirection that makes the code harder to trace.

---

## Principle 9: Use Theoretical Frameworks Exhaustively

### UI Version
"Colour wheel, not gut feel. Grid systems, not eyeballing."

### Architecture Equivalent
**When a decision has an established analytical framework, apply it systematically rather than relying on intuition.** CAP theorem, ACID vs BASE trade-offs, the Universal Scalability Law, Amdahl's Law, queueing theory -- these are not academic exercises. They are precision instruments that eliminate guesswork.

### Before/After Example

**Before (gut feel):** "We need the payment service to be highly available, strongly consistent, and partition-tolerant." The team builds it with synchronous replication across 3 data centres and expects all three properties simultaneously. In production, a network partition between data centres causes the system to hang: it refuses to serve stale data (consistency) but cannot reach the primary (partition). The system is unavailable. CAP theorem predicts this outcome exactly -- you cannot have all three during a partition. The team did not apply the framework.

**After (framework applied):** The team applies CAP analysis explicitly:
- During normal operation: strong consistency via synchronous replication.
- During partition: the system switches to serving stale reads with a "data may be delayed" banner (AP mode) while writes queue for reconciliation.
- After partition heals: queued writes replay, consistency restores.

This is a deliberate CP-to-AP failover strategy, not a surprise. The framework told them the trade-off was unavoidable, so they designed for it instead of being surprised by it.

### Anti-Pattern Prevented
**Wishful architecture.** Assuming you can have consistency AND availability AND partition tolerance simultaneously, or that your system will scale linearly without contention, because you have not applied the framework that would tell you otherwise.

---

## Principle 10: Every Parameter Gets Exactly One Home

### UI Version
"Duplicates create cognitive confusion. The user sees the same value in two places and wonders: which is the source of truth?"

### Architecture Equivalent
**Every business rule, configuration value, and source of truth must live in exactly one place. The difference between "don't repeat the code" and "don't repeat the decision" is critical: duplicating a utility function is inconvenient; duplicating a business rule is dangerous.**

### Before/After Example

**Before (three homes for one rule):** A "free shipping over $50" rule exists in:
1. The frontend JavaScript: `if (cartTotal > 50) { shippingCost = 0; }`
2. The backend order service: `if order.total > Decimal('50.00'): order.shipping = Decimal('0')`
3. A database trigger: `IF NEW.total > 50 THEN NEW.shipping_cost := 0; END IF;`

Marketing changes the threshold to $75. The backend is updated. The frontend is forgotten. The database trigger is forgotten (nobody remembers it exists). Result: the frontend shows "Free Shipping!" for $60 orders. The backend charges shipping. The database trigger refunds it. Three sources of truth, three different behaviours. The customer sees free shipping, gets charged, then sees a mysterious credit.

**After (one home):** The shipping rule lives in the domain service, period. The frontend calls the API to compute shipping cost. The database stores the result. Neither the frontend nor the database contains the business rule. Changing the threshold requires one edit in one file. The decision lives in one home.

### Anti-Pattern Prevented
**Distributed truth.** The same business decision replicated across layers, services, or systems. Each replica drifts independently. The system's behaviour becomes the emergent interaction of multiple divergent copies of the same rule, which is non-deterministic and unfixable without consolidation.

---

## Principle 11: Let Constraints Kill Options

### UI Version
"Physical encoder stacking killed encoder assignment for top-zone params."

### Architecture Equivalent
**State all constraints before generating options. Constraints are not limitations to work around -- they are filters that eliminate wrong options before you waste time evaluating them.** "We must support offline mode" kills architectures requiring constant connectivity. "We have 2MB of SRAM" kills designs assuming unlimited heap. "The team has 3 engineers" kills microservices requiring 10 deployment pipelines.

### Before/After Example

**Before (constraints discovered late):** A team designs a microservices architecture with 8 services, each with its own database, deployed on Kubernetes. After 3 months of design and initial implementation, they discover:
- Constraint: the operations team is 1 person (kills Kubernetes -- nobody to maintain it)
- Constraint: the product must run on-premises at customer sites (kills managed cloud services)
- Constraint: customers have intermittent internet (kills architectures requiring service discovery via cloud DNS)

Three months of design work invalidated by constraints that were known but not surfaced.

**After (constraints stated first):**
- 1-person ops team -> must be operationally simple (single binary or docker-compose, not Kubernetes)
- On-premises deployment -> must be self-contained (no cloud service dependencies)
- Intermittent internet -> must function offline (no external API calls in the critical path)

These three constraints kill microservices, kill cloud-native, and kill SaaS-dependent architectures. What survives: a modular monolith, deployed as a single Docker container, with an embedded database (SQLite or embedded Postgres), and optional cloud sync when connectivity is available. The constraints did the architectural work. The team did not waste 3 months on a dead end.

### Anti-Pattern Prevented
**Constraint-blind design.** Generating and evaluating architectures without first establishing the hard constraints that eliminate most options. This leads to sunk-cost attachment to designs that were never viable.

---

## Principle 12: Match Control Precision to Parameter Resolution

### UI Version
"A parameter with 256 values but only 4 useful positions does not need an encoder. It needs a toggle."

### Architecture Equivalent
**Match the communication pattern to the actual data flow resolution.** REST for request-response CRUD. WebSocket for bidirectional real-time streams. Message queue for asynchronous fire-and-forget. gRPC for low-latency inter-service calls. Server-Sent Events for unidirectional server push. Using Kafka for 10 messages per minute is an encoder for a 4-position parameter. Using HTTP polling every 100ms for real-time data is a toggle for a continuous parameter.

### Before/After Example

**Before (over-engineered):** A team uses Apache Kafka with Avro schema registry, 12 partitions, and consumer group management for an internal audit log that receives 50 events per day. Operational overhead: ZooKeeper/KRaft cluster, schema evolution management, consumer lag monitoring, partition rebalancing. The team spends more time maintaining Kafka than writing business logic. The audit log has no ordering requirements, no replay needs, and no real-time consumers.

**After (matched precision):** The audit events are written to an append-only PostgreSQL table with a BRIN index on timestamp. A nightly job exports to S3 for long-term retention. Zero new infrastructure. Zero operational overhead. The data flow resolution (50 events/day, batch consumption, no ordering) matched to the simplest tool that handles it.

**Before (under-engineered):** A dashboard polls a REST API every 2 seconds to check if new alerts have arrived. 1800 requests per hour per client, 50 clients = 90,000 requests per hour. 99.8% of responses are "no change." The API server scales to handle the load. The actual requirement: push an alert to the dashboard within 500ms of it occurring. That is a real-time push pattern.

**After (matched precision):** The dashboard opens a WebSocket connection. The server pushes alerts when they occur. Connection count: 50 persistent sockets. Request volume: near zero (only actual alerts). Latency: sub-100ms. The communication pattern now matches the data flow resolution.

### Anti-Pattern Prevented
**Impedance mismatch.** Using a high-ceremony communication pattern for a low-resolution data flow (over-engineering), or a low-capability pattern for a high-resolution flow (under-engineering). Both waste resources -- one wastes infrastructure, the other wastes bandwidth.

---

## Applied: Choosing a Database

The 12 principles applied to the PostgreSQL vs MongoDB vs Redis vs SQLite decision:

| Principle | Application |
|-----------|-------------|
| #1 Diagnose the disease | "We need NoSQL" might be a schema flexibility problem (use JSONB in Postgres), a write throughput problem (use partitioning), or an operational simplicity problem (use SQLite). Diagnose first. |
| #2 Research the right domain | Study your actual access patterns (read/write ratio, query complexity, transaction boundaries) -- not database feature matrices. |
| #4 Preserve what works | If your team has 5 years of PostgreSQL expertise and the current Postgres instance handles load, do not migrate to MongoDB because a blog post said so. |
| #7 Generate more options | Evaluate at least: Postgres, Postgres+JSONB, SQLite, MongoDB, and a polyglot combination. Do not frame it as binary. |
| #9 Use frameworks | Apply CAP theorem: do you need CP (Postgres) or AP (Cassandra, DynamoDB)? Apply the data model framework: is your data relational, document, graph, or time-series? |
| #10 One home | One database should be the source of truth for each entity. If user data exists in both Postgres and a Redis cache, Postgres is the home. Redis is a projection. Never treat both as authoritative. |
| #11 Constraints kill options | "Must run on a Raspberry Pi" kills Oracle. "Must handle 1M writes/sec" kills SQLite. "Team of 1" kills anything requiring a DBA. "Must support ACID transactions across 5 tables" kills most NoSQL options. |
| #12 Match precision | If your data is 90% reads with complex JOINs, Postgres. If it is 90% writes with simple key-value access, DynamoDB/Redis. If it is 500 rows that never change, SQLite or even a JSON file. Do not use a Ferrari to go to the letterbox. |

---

## Applied: Microservices vs Monolith

| Principle | Application |
|-----------|-------------|
| #1 Diagnose the disease | "We need microservices because the monolith is slow" -- is it actually slow everywhere, or slow in one expensive query? Is the problem scaling, deployment velocity, or team autonomy? Each diagnosis leads to a different architecture. |
| #2 Research the right domain | Study your team structure, deployment frequency, and scaling bottlenecks -- not Netflix's architecture. Netflix has 2,000 engineers. You have 4. |
| #4 Preserve what works | The monolith's transactional consistency, simple deployment, and single-process debugging are features, not legacy. Preserve them unless you have a specific reason to give them up. |
| #5 Three levels of hierarchy | Monolith: presentation / domain / infrastructure layers. Microservices: API gateway / domain services / data stores. Either way, you need at least three levels with enforced boundaries. A monolith without layers is mud. Microservices without clear domain boundaries are a distributed monolith. |
| #10 One home | Each business capability has one owning service/module. "Who owns inventory?" must have exactly one answer. If both the order service and the warehouse service can modify inventory, you have a distributed truth problem. |
| #11 Constraints kill options | "Team of 3" kills 8-service architectures (nobody to maintain them). "Sub-millisecond cross-module latency required" kills network-separated services. "Must deploy as a single binary to air-gapped environments" kills microservices entirely. State the constraints, watch options die. |
| #12 Match precision | If only one module needs independent scaling, extract only that module. Do not decompose the entire monolith because one endpoint gets 100x the traffic. Match the architectural precision to the actual scaling resolution. |

---

## Applied: API Versioning

| Principle | Application |
|-----------|-------------|
| #4 Preserve what works | Existing API consumers rely on the current contract. Breaking changes destroy their working integrations. Versioning exists to preserve what works for existing clients while evolving for new ones. |
| #10 One home | The canonical API behaviour must live in one place. If v1 and v2 share a codebase with `if (version == 1)` branches scattered through 40 controllers, the "version" has no single home -- it is distributed across the entire codebase. Better: v1 is a thin adapter layer that maps to the current domain model. The domain model is the one home. Versioned adapters translate. |
| #11 Constraints kill options | "Clients cannot be forced to upgrade" kills breaking changes without versioning. "Team cannot maintain 3 concurrent API versions" kills per-minor-version URLs. "Mobile apps have 6-month update cycles" means v1 must be supported for at least 12 months after v2 ships. These constraints determine the versioning strategy before you even discuss URL paths vs headers. |
| #6 Sub-decisions | "We will version the API" is level 1. Level 2: URL path (`/v2/`) vs header (`Accept: application/vnd.api+v2`) vs query param (`?version=2`). Level 3: what is a "breaking change" -- adding a field? Removing a field? Changing a field type? Renaming an endpoint? Each sub-decision must be evaluated and documented separately. |

---

## Applied: The main.cpp Decomposition (Tab5-Encoder)

The tab5-encoder `main.cpp` began as a single 2,500+ line file containing I2C initialisation, WiFi management, WebSocket routing, encoder handling, display UI, touch handling, parameter management, preset storage, LED feedback, and OTA updates. The decomposition extracted these into `input/`, `network/`, `ui/`, `parameters/`, `presets/`, `storage/`, `config/`, `hal/`, `hardware/`, `utils/`, and `zones/`.

Here is how each principle maps to the actual decisions made:

| # | Principle | Followed? | Evidence |
|---|-----------|-----------|----------|
| 1 | Diagnose the disease | **Yes** | The problem was not "main.cpp is too long" (symptom). The problem was that unrelated concerns (WiFi, encoders, display, presets) were coupled in a single compilation unit, making any change to one subsystem risk breaking another. The disease was coupling, not length. |
| 2 | Research the right domain | **Yes** | The decomposition followed embedded systems patterns (hardware abstraction in `hal/`, input device drivers in `input/`, network transport in `network/`) rather than applying web-framework patterns (MVC) that would have been the wrong domain for an ESP32-P4 controller. |
| 3 | Structure before visuals | **Partially** | The directory structure was proposed as a module hierarchy (structure first). But the boundaries were not diagrammed or formally evaluated before implementation -- the decomposition was done in-place rather than designed on paper first. A C4 component diagram would have caught the remaining coupling between `main.cpp` and its 30+ includes. |
| 4 | Preserve what works | **Yes** | The decomposition preserved the existing initialisation sequence (`setup()` ordering), the WebSocket message protocol, the encoder addressing (Unit A @ 0x42, Unit B @ 0x41), and the I2C recovery mechanism. These were working. Only the code organisation was changed, not the runtime behaviour. |
| 5 | Three levels of hierarchy | **Partially** | The decomposition created clear directories (good), but `main.cpp` still contains global state declarations (`g_encoders`, `g_wifiManager`, `g_wsClient`, `g_paramHandler`, etc.) that blur the boundary between the orchestration layer and the service layer. A stricter three-level hierarchy would be: `main.cpp` (orchestration only) -> services (each self-contained with private state) -> hardware abstraction. The current state has two levels: `main.cpp` globals + extracted modules. |
| 6 | Test sub-decisions | **Partially** | "Extract WiFi management" was level 1. Level 2: should WiFiManager own the connection state machine, or should main.cpp drive it? The answer (WiFiManager owns the FSM) was correct but was discovered during implementation rather than evaluated beforehand. Level 3: should WiFiManager expose events or callbacks? The answer (callbacks) was also discovered during implementation. |
| 7 | Generate more options | **No** | There is no evidence that multiple decomposition strategies were generated and compared. For example: option A (extract by hardware subsystem), option B (extract by lifecycle phase: init/run/shutdown), option C (extract by communication boundary: local vs network). Only option A was pursued. It happened to be the right one for this domain, but it was not validated by comparison. |
| 8 | Every element carries information | **Mostly** | Most extracted modules carry distinct responsibilities. However, `utils/` contains only 14 lines and `zones/` only 34 lines. These are structural decoration at their current size -- they exist as directories for future content rather than carrying meaningful current responsibility. They should have remained in their nearest neighbour until they grew large enough to justify extraction. |
| 9 | Use frameworks exhaustively | **No** | No formal dependency analysis, coupling metrics, or module cohesion frameworks were applied. The decomposition was done by human judgement ("WiFi code goes in network/"), which happened to produce reasonable boundaries but was not validated by a framework like Structured Analysis or a dependency matrix. |
| 10 | One home per parameter | **Partially** | Most concerns have one home after decomposition. However, `main.cpp` still contains effect name caches (`s_effectNames`, `s_effectKnown`, `s_k1EffectName`, `s_k1EffectId`) and palette name caches (`s_paletteNames`, `s_paletteKnown`) that arguably belong in a `CatalogueCache` or in the `ParameterHandler`. These are parameters with two conceptual homes: they are declared in `main.cpp` but consumed by the UI and network layers. |
| 11 | Let constraints kill options | **Yes** | ESP32-P4 constraints shaped the decomposition: single-threaded Arduino `loop()` means no inter-service message passing (kills actor model). Limited SRAM means no dynamic module loading (kills plugin architecture). I2C bus shared with display means hardware abstraction must isolate bus access (kills direct-register patterns in business logic). These constraints were respected. |
| 12 | Match control to resolution | **Yes** | The decomposition granularity matches the actual change frequency. `network/` changes when the WebSocket protocol changes. `input/` changes when encoder hardware changes. `ui/` changes when the display layout changes. These change independently and at different rates, which is exactly why they are separate modules. A finer decomposition (separate files for each encoder, separate files for each UI widget) would have been over-precise for the current 142-file codebase. |

### Summary Scorecard

| Verdict | Count | Principles |
|---------|-------|------------|
| Followed | 5 | #1, #2, #4, #11, #12 |
| Partially followed | 4 | #3, #5, #6, #10 |
| Not followed | 2 | #7, #9 |
| Not applicable | 1 | #8 (marginal -- `utils/` and `zones/` are borderline) |

### Recommended Next Steps

1. **#10 violation:** Extract effect/palette name caches from `main.cpp` into a `CatalogueCache` module. These 80+ lines of state and logic have no business in the orchestration file.
2. **#5 violation:** Eliminate global state (`g_encoders`, `g_wifiManager`, etc.) from `main.cpp`. Each service should own its state privately, with `main.cpp` holding only the orchestration sequence and pointers/references to services.
3. **#3 gap:** Before the next major decomposition (e.g., extracting the remaining `main.cpp` logic into a `SystemOrchestrator`), draw a C4 component diagram showing all module dependencies and evaluate it for circular dependencies before writing code.
4. **#8 gap:** Fold `utils/` (14 LOC) and `zones/` (34 LOC) back into their parent modules until they grow to justify their own directories. Empty or near-empty directories are promises, not architecture.

---

*Documented: 2026-03-22*
*Derived from DESIGN_DECISION_PROCESS.md (UI parameter layout exercise) applied to software architecture decision-making.*

---

**Document Changelog**
| Date | Author | Change |
|------|--------|--------|
| 2026-03-22 | agent:software-architect | Created -- mapped 12 UI design principles to architecture equivalents with concrete examples, applied to database selection, monolith-vs-microservices, API versioning, and the tab5-encoder main.cpp decomposition |
