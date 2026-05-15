# Church Treasurer System – Architecture & Development Roadmap
**Hope Baptist Church of Nashville, Georgia**  
**Revision: 1.3**  
**Date: 29 April 2026**

**Aligned with:**
- Treasurer’s Guide (Conceptual Edition .9b)

---

## 1. Architecture Overview

### Purpose
This document defines the high-level architecture and serves as the **single source of truth** for all implementation decisions in the development thread. It ensures the system faithfully implements the **Treasurer’s Guide** while meeting the Treasurer’s practical needs: native-speed primary tool, centralized data, LAN portability during remodeling, and simple read-only access for authorized users.

### Core Requirements & Constraints
- Primary user (Treasurer) needs full read/write capability with a fast, native desktop experience.
- Other users (board, deacons, staff) need read-only access via browser on the local church network.
- Data must be centralized on the church Linux server.
- No public internet exposure — everything stays on the LAN (or tunneled).
- Must strictly follow fund accounting principles from the Treasurer’s Guide (WODR / WDR, releases from restrictions, functional + natural classification, donor stewardship, audit-readiness).
- Development will use the existing local LLM setup (Qwen3.5 35B-A3B MoE Q4_K_M in Continue.dev on Kubuntu).

### High-Level Architecture
Church Server (Linux) 
├── MariaDB (central database) 
├── Nginx + PHP-FPM (read-only web interface – LAN only)

Treasurer’s Machines (any Linux laptop/desktop on LAN) 
└── C++ Qt6 Desktop Application (full read/write) 
└── Connects directly to MariaDB via QSqlDatabase


**Data Flow**:  
All transactions, adjustments, and budget entries are created **only** through the C++ Qt application.  
The read-only web interface queries the same database with a restricted DB user.

### Tech Stack Summary
- **Database**: MariaDB 10.11+
- **Primary Tool**: C++ Qt6 Widgets desktop app
- **Charting**: Qt Charts
- **Web**: Plain PHP + Bootstrap 5 (read-only)
- **Distribution**: AppImage (primary) + Debian package

---

## 2. Detailed Development Roadmap

### Overall Project Goals
- Deliver a production-ready, audit-focused treasurer tool by **Q4 2026**.
- Primary tool: Fast native C++ Qt6 desktop application.
- Secondary tool: Secure LAN-only read-only web dashboard.
- Strictly follow Treasurer’s Guide principles (accounting logic lives only in the Guide and is enforced via Temper Prompt).

### Phase 0: Project Setup & Foundations (1–2 weeks)
- Initialize private Git repository.
- Create full project skeleton (`CMakeLists.txt`, directory structure, README, DEVELOPMENT.md, .gitignore, LICENSE).
- Set up build environment on Kubuntu.
- Define versioning & branching strategy.
- Basic CI linting (clang-format, etc.).

**Deliverables:** 
- Compilable AppImage that launches a blank main window and successfully connects to a test MariaDB instance.
- **Initial User Experience Outline**  (main menu + top 5–6 screens)

### Phase 1: Database Layer (2–3 weeks)
- Design and implement complete MariaDB 10.11+ schema (single database with `fiscal_year` column).
- Core tables: `funds`, `transaction_headers`, `transaction_lines`, `net_assets_tracking`, `releases_from_restrictions`, `budgets`, `budget_lines`, `audit_log`, etc.
- Create two database users: full-privilege service account (Qt app) + dedicated read-only user (web).
- Seed initial funds based on Treasurer’s Guide categories.
- Migration scripts and basic validation queries.
- All schema objects documented with references to relevant Treasurer’s Guide sections.

**Milestone:** Schema deployed on church server; basic SQL CRUD operations tested.

### Phase 2: Core Accounting Engine (C++) (3–4 weeks)
- Implement domain classes in `src/core/`: `Fund`, `Transaction`, `TransactionLine`, `LedgerEngine`, `ReleaseFromRestriction`, `Budget`, etc.
- Double-entry validation, restriction enforcement, release-from-restrictions logic, classification helpers.
- Balance calculation engines (WODR / WDR).
- Unit tests covering all key rules.

**Milestone:** Engine passes full reconciliation test suite against sample data.

### Phase 3: Qt Desktop Application – Data Access & UI (4–5 weeks)
> **NOTE:** *Detailed UI/UX design and screen specifications will be completed during Phase 0 and early Phase 1 before heavy widget implementation.

- Database repository layer using QSqlDatabase + prepared statements.
- Configuration & encryption with QtKeychain.
- Main UI windows (Dashboard, Transaction/Journal entry, Fund management, Budget entry & tracking, Reports viewer).
- Validation tied directly to core engine.

**Milestone:** Functional desktop app capable of complete transaction lifecycle on LAN.

### Phase 4: Reporting & Export (2–3 weeks)
- Qt Charts integration for dashboards.
- PDF generation (QPrinter + QTextDocument or QPdfWriter).
- Key reports: Budget-to-actual, fund balances, functional expense summaries, restricted fund status.
- CSV/JSON export support.

**Milestone:** All board-required reports exportable and printable.

### Phase 5: Web Read-Only Interface (2–3 weeks, parallelizable)
- Nginx + PHP-FPM setup (LAN-only).
- Plain PHP + Bootstrap 5 pages with PHP native sessions.
- Dashboard, fund balances, and report views using read-only DB user.
- Security hardening.

**Milestone:** Authorized users can browse current financials via browser on church network.

### Phase 6: Packaging, Deployment & Polish (2 weeks)
- AppImage build recipe (primary).
- Debian package creation.
- Installation scripts and documentation.
- Logging, error handling, backup recommendations.
- Treasurer user guide.

### Phase 7: Testing, Documentation & Handover (2–3 weeks)
- Integration testing against Treasurer’s Guide scenarios.
- Full audit-trail verification.
- Complete user and technical documentation.
- Training materials for board/deacons.

### Timeline Summary
- **Total Duration:** Approximately 4–5 months (part-time, LLM-assisted development).
- **Key Dependency:** Treasurer’s Guide must be stable before heavy Phase 2 work.

### Risks & Mitigation
- Keep vertical slices small — each phase produces a working artifact.
- Use Temper Prompt for any accounting logic snippets.

---

## 3. Locked Technical Decisions (Decision Matrix)

- **Database Engine**: MariaDB 10.11+ (**Locked**)
- **C++ Qt GUI Approach**: Qt6 Widgets (**Locked**)
- **Charting / Reporting Library**: Qt Charts (**Locked**)
- **Application Distribution**: AppImage + Debian package (start with AppImage) (**Locked**)
- **Read-Only Web Templating**: Plain PHP + Bootstrap 5 (**Locked**)
- **Web Authentication**: PHP native sessions + login form (**Locked**)
- **Qt Configuration Storage**: QtKeychain (**Locked**)
- **Fiscal Year / Archiving**: Single database with `fiscal_year` column (**Locked**)
- **Database User Strategy**: Hard-coded service account (encrypted with QtKeychain) for Qt app + read-only user for web (**Locked**)

**Minor / Future Items** (decided later):
- Exact Qt version (6.7 vs 6.8)
- QML vs Widgets for specific screens
- Plugin system type
- PDF report library choice

---
### 4. Extensibility Strategy (Modules)

The system will implement a lightweight **Module Framework** to support future growth without compromising the stability of the core application.

**Core Principles:**

- Major functional areas (Payroll/Staff Compensation, Advanced Member Integration, Benevolence Manager, Missions Tracking, Custom Reporting, etc.) will be developed as independent **Modules**.
- A simple ModuleManager and base Module class will be introduced in Phase 2.
- Modules can register their own menu items, database tables, UI components, and dashboard widgets.
- Initial implementation will be **compile-time** (statically linked) for simplicity and reliability. Dynamic loading (shared libraries) can be added later.
- This allows the team to refine and test each major feature independently once the core system is stable.
- The core application will only contain: Dashboard, Basic Transactions, Funds & Accounts, Budgeting, and standard Reports.

**Plugin System** (Python / scripting) will be considered separately in a future version for lightweight extensions and custom reports.

This modular approach aligns with our goal of building a maintainable, long-term solution for Hope Baptist Church.


---

**Approval Status:** Merged v1.3 — Ready for active development.
