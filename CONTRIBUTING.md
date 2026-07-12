# Contributing to LightwaveOS

Guidelines for contributing to the public LightwaveOS repository.

## Licence

This project is licensed under the [Apache License 2.0](LICENSE). By contributing, you agree that your contributions will be licensed under the same terms.

## Developer Certificate of Origin

All commits must be signed off to certify that you have the right to submit them under the project's licence:

```text
Signed-off-by: Your Name <your.email@example.com>
```

Use `git commit -s` to add the sign-off automatically.

## Public Repository Hygiene

Do not commit local credentials, private network names, API tokens, generated build outputs, local agent configuration, private music corpora, or temporary research artefacts. Keep local overrides in ignored files such as `wifi_credentials.ini`, `.env.local`, or `network_config_private.h`.

Generated binaries, cache directories, and personal IDE state should remain outside Git. If a fixture or corpus payload is required for tests, document how to obtain or regenerate it without committing private or third-party media.

## Pull Requests

1. Create a branch from `main`.
2. Keep changes scoped to one logical unit.
3. Include the build or test command used to verify the change.
4. Sign off commits with the DCO line above.
5. Open a pull request with a concise summary and any remaining risks.
