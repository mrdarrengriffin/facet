---
title: "List parts for the GitHub screen"
date: "2026-10-22"
time: "21:50"
area: "Components"
image: "2026-10-22-list-parts.png"
alt: "Pull request rows with their review and CI chips"
links: [["Pull request row", "components/pr_row/"], ["QR block", "components/qr_block/"]]
---
The last list parts now run on the panel renderer: state chips for review and CI, the pull request row, the check row for one CI check, a quiet empty state, and a QR block that stays dark on light in both themes so phones can scan it. Each is built from the components that were already there (status chip, status dot, icons), so a change to one shows up everywhere.
