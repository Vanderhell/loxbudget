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

Include, when possible:

- A description of the issue and potential impact
- Steps to reproduce
- Affected version, commit, configuration, or feature flags
- Any suggested fix or mitigation

If you are unsure whether something is security-sensitive, report it privately first.

## What counts as a security issue

Security-sensitive reports include, for example:

- Fail-open behavior that bypasses declared safety or admission policies
- Memory corruption, undefined behavior, or state corruption reachable from public API usage
- Incorrect enforcement of resource, pressure, or lifetime limits in a way that can be exploited in deployment
- Release or CI pipeline issues that could compromise published artifacts

The following are usually not security issues by themselves unless they create a realistic exploit path:

- Documentation mistakes
- Build failures without security impact
- Feature requests or API design disagreements
- Performance regressions without safety or isolation impact

## Coordinated disclosure

- Please allow time for investigation and remediation before public disclosure.
- The maintainer will make a best-effort attempt to validate the report, assess impact, and coordinate a fix.
- Once a fix is available, the project may disclose the issue publicly in release notes or a security advisory.

## Response expectations

- Initial acknowledgment target: within 7 days
- Best-effort remediation or triage update: within 30 days

## Artifact trust

Official release artifacts are those published from this repository's tagged releases and attached by the GitHub release workflow. If you suspect release artifact tampering or supply-chain compromise, report it through the private channels above.
