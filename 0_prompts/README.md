# Prompts given to the agents

The task texts given to the agents, verbatim as recorded by the authors. File paths are replaced by
placeholders in square brackets ([INSTANCE FILE], [OUTPUT FILE], ...); everything else, including
typos, is unchanged.

| File | Runs (paper, Table 3) | Type |
|---|---|---|
| `1_jssp_tai_100_1000_1_autonomous_task.md` | autonomous job-shop sessions on `tai_100_1000_1` (Claude Opus 5 / Sonnet 5), mode A | complete task text; the agent received nothing else besides the instance file |
| `2_fjsp_behnke16_autonomous_task.md` | autonomous sessions on `behnke16` (Claude Opus 5), mode A | complete task text, plus the instance file and a small example instance and schedule |
| `3_fjspt_assisted_session_first_message.md` | assisted transport-robot session (Claude Opus 5.5), mode B | first message of an interactive session; later goals, feedback and the papers the agent requested were given during the session |

The paper of Meng et al. (2023) pasted into the third prompt is not redistributed here; it is
Sensors 23(8):3815, doi:10.3390/s23083815 (open access).

The other runs of the paper (the two autonomous sessions on behnke14/16/31, the job-shop session that also
received earlier code and the best value then known, the assisted job-shop and Behnke sessions, and the two
campaigns of mode C) used the same or very similar task texts.
