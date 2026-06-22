This directory contains a legacy Perl model verifier for Egoboo.

It verifies that all frames of the found models are what Egoboo expects.

It requires Perl and is not part of the main CMake build. The active content
verification path is `egoboo-content-validator`.

## Usage

`perl modelverifier.pl [-no-warnings] [-no-list] <path to Egoboo's data>`

`--no-warnings` turns off warnings.

`--no-list` turns off the list of warnings and errors.
