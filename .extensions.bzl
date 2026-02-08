"""Module extension for SPSCQueue (header-only, not in BCR)."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def _spscqueue_impl(ctx):
    http_archive(
        name = "spscqueue",
        url = "https://github.com/rigtorp/SPSCQueue/archive/refs/tags/v1.1.tar.gz",
        strip_prefix = "SPSCQueue-1.1",
        build_file = "//third_party:spscqueue.BUILD",
        sha256 = "",  # Bazel will warn on first fetch; pin after verifying
    )

spscqueue = module_extension(implementation = _spscqueue_impl)
