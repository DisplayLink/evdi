Before submitting the PR please make sure you have run *ci* scripts:
  * `./ci/build_against_kernel` (see `--help` for all options)
    We need backward compatibility and this script is a handy way of testing
    build compliance with many kernel versions.
  * `./ci/run_style_check`
    We want to be as style compliant as possible. Again, this is a handy way of
    checking it.

If you have more than one change consider creating separate PRs for them; it
will make the review process much easier. Also provide some description (links
to source code or mailing list are welcome - this might help to understand the
change).

Thanks for the contribution!
