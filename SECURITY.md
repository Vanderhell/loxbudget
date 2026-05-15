# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| `v1.0.x` | yes |
| pre-`v1.0` | best effort |

## Reporting

If you believe you have found a security vulnerability, do not open a public issue.

Report privately to the maintainer via:

- GitHub Security Advisory or private GitHub contact
- Email: `Vanderhell@gmail.com`

Include:

- A description of the issue and potential impact
- Steps to reproduce
- Any suggested fix or mitigation

## What counts as a security issue

Security-sensitive reports include, for example:

- Fail-open behavior that bypasses declared safety or admission policies
- Memory corruption, undefined behavior, or state corruption reachable from public API usage
- Incorrect enforcement of resource, pressure, or lifetime limits in a way that can be exploited in deployment
- Release or CI pipeline issues that could compromise published artifacts

## Response expectations

- Initial acknowledgment target: within 7 days
- Best-effort remediation or triage update: within 30 days
