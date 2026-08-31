class FsLintLegibility < Formula
  source_url = "https://github.com/yowainwright/fs-lint-legibility/releases/download/v0.2.0/" \
               "fs-lint-legibility_0.2.0_source.tar.gz"
  source_sha = "TODO_AFTER_V0_2_0_RELEASE"

  desc "Filesystem linting for proposed files"
  homepage "https://github.com/yowainwright/fs-lint-legibility"
  url source_url
  sha256 source_sha
  license "MIT"

  livecheck do
    url :stable
    strategy :github_latest
  end

  depends_on "cmake" => :build

  def install
    system "cmake", "-S", ".", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build", "--parallel"
    system "cmake", "--install", "build"
  end

  test do
    assert_match "fs-lint 0.2.0", shell_output("#{bin}/fs-lint --version")
  end
end
