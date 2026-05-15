# Local LLM Setup Reference – Hope Baptist Church Treasurer Workspace

**Purpose:**  
This document records the agreed-upon local LLM configuration for AI-assisted coding and Treasurer Assistant tasks in VSCodium on Kubuntu Linux. It is maintained separately from the Treasurer’s Guide and Temper Prompt.

**Last Updated:** April 22, 2026  
**Thread:** Local LLM Setup and Configuration (exclusive)

## System Specifications (Restored from Full Conversation)
- **Operating System:** Kubuntu Linux
- **Editor:** VSCodium (open-source VS Code fork)
- **Hardware:**
  - **CPU:** AMD Ryzen 9 5900XT (16 cores / 32 threads, Zen 3 architecture, up to 4.8 GHz boost, AM4 platform with DDR4 RAM)
  - **GPU (Primary):** NVIDIA RTX 5060 Ti 16 GB VRAM (Blackwell architecture) — preferred for LLM inference due to newer kernels and better performance on 30B–35B class models
  - **Secondary GPU Available:** NVIDIA RTX A4000 16 GB GDDR6 ECC (Ampere architecture, 140W TDP, single-slot) — usable as alternative or for testing/multi-GPU, but generally slower than the 5060 Ti for this workload
  - **Key Constraints:** DDR4 memory subsystem (slower offload bandwidth vs DDR5); 16 GB VRAM limits comfortable full-GPU runs to ~30–35B class models at Q4_K_M with some CPU offload on larger/MoE variants; no production time pressure (personal learning project)

## Core Stack
- **Inference Engine:** Ollama (local, OpenAI-compatible API, GPU offloading enabled via CUDA)
- **Primary AI Integration:** Continue.dev extension in VSCodium (inline autocomplete, sidebar chat, edit/agent mode)
- **Optional Companion:** Aider (terminal-based for larger refactors)

## Installed & Tested Models (Restored from Conversation)
- **Primary Target Model:** Qwen3.5 35B-A3B MoE (or similar Qwen3 / Qwen3.5 32B–35B class) at **Q4_K_M quantization**
  - Successfully discussed and pulled/installed in the 32B–35B range; 35B-A3B MoE recommended as efficient high-quality option (~13–24 GB file size, light-to-medium offload on your hardware)
  - Pull command: `ollama pull qwen3.5:35b-a3b-q4_K_M` (or exact tag like `qwen3.5:35b` / community coder variants)
  - Why chosen: Strong reasoning, coding, and agentic performance for full-app architecture, refactoring, and church accounting software design (double-entry logic, schemas, security). Acceptable speed on your setup since time is not critical.
- **Supporting Models:**
  - Fast autocomplete: `qwen2.5-coder:1.5b-base` or `qwen2.5-coder:14b-instruct` (your earlier baseline, ~9 GB)
  - Embeddings: `nomic-embed-text`
- **Quantization Preference:** Stick with **Q4_K_M** (best VRAM/speed balance on 16 GB). Q5_K_M viable for smaller models; FP8 discussed but not recommended due to higher VRAM cost with limited quality gain for daily coding.

## GPU Discussion Summary (Restored)
- **RTX 5060 Ti 16 GB (Primary):** Newer Blackwell architecture → better tokens/sec, tensor core efficiency, and modern kernel support. Preferred card for 30B–35B MoE models.
- **RTX A4000 16 GB:** Solid Ampere workstation card with ECC memory (more reliable for long runs) and lower power draw. Usable via `CUDA_VISIBLE_DEVICES` or physical swap, but generally 10–30% slower than the 5060 Ti on the same models due to older architecture.
- Multi-GPU possible but optional (Ollama can split layers). No immediate need — 5060 Ti is the stronger performer for this workload.
- Offload behavior: Larger models spill some layers/experts to Ryzen 9 5900XT + system RAM. Your 16-core CPU handles it decently; monitor with `nvidia-smi` and `htop`.

## Recommended Continue.dev Configuration (`~/.continue/config.yaml`)
```yaml
models:
  - name: Qwen3.5 35B-A3B Q4 (Main Coding Model)
    provider: ollama
    model: qwen3.5:35b-a3b-q4_K_M          # or exact pulled tag (e.g. qwen3.5:35b)
    apiBase: http://localhost:11434
    roles:
      - chat
      - edit
      - apply
    defaultCompletionOptions:
      temperature: 0.5                    # Updated for more deterministic financial/logic code
      topP: 0.9                           # Updated
      contextLength: 16384                # Updated (increased from 8192)

  # Fast autocomplete (keeps inline suggestions responsive)
  - name: Qwen2.5-Coder 1.5B (Autocomplete)
    provider: ollama
    model: qwen2.5-coder:1.5b-base
    apiBase: http://localhost:11434
    roles:
      - autocomplete

embeddings:
  provider: ollama
  model: nomic-embed-text
  apiBase: http://localhost:11434

allowAnonymousTelemetry: false