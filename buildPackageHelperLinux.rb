#!/usr/bin/env ruby
# PackageHelper Native Compilation Script
# Compiles PackageHelper_v15 native libraries (.so for Linux)
# Usage: ruby buildPackageHelperLinux.rb [amd64|arm64|x86|all]

require 'fileutils'
require 'rbconfig'

SCRIPT_DIR = File.dirname(File.expand_path(__FILE__))
Dir.chdir(SCRIPT_DIR)

PH_VERSION = "v15"
OUTPUT_DIR = "Output"
SDK_DIR_NAME = "OldUnreal-UT99-ACE-SDK"

def colorize(text, color_code)
  "\e[#{color_code}m#{text}\e[0m"
end

def green(text);  colorize(text, 32); end
def yellow(text); colorize(text, 33); end
def red(text);    colorize(text, 31); end
def cyan(text);   colorize(text, 36); end

def header(text)
  puts ""
  puts cyan("=" * 60)
  puts cyan(" #{text}")
  puts cyan("=" * 60)
end

def macos?
  RbConfig::CONFIG['host_os'] =~ /darwin/
end

def docker_available?
  system("docker info > /dev/null 2>&1")
end

def build_linux_so(arch)
  header "Building PackageHelper_#{PH_VERSION}.so for Linux #{arch}"

  extra_packages = ""
  extra_cmake_flags = ""

  case arch
  when "amd64", "x86_64"
    output_subdir = "System64"
    docker_platform = "linux/amd64"
  when "x86", "i386"
    output_subdir = "System"
    docker_platform = "linux/amd64"
    # Real 32-bit build: needs multilib and -m32 so the SDK's 32-bit
    # Core.so/Engine.so in System/ get picked up by CMake's CPU check
    extra_packages = "gcc-multilib g++-multilib"
    extra_cmake_flags = "-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32"
  when "arm64", "aarch64"
    output_subdir = "SystemARM64"
    docker_platform = "linux/arm64"
  else
    puts red("Unknown architecture: #{arch}")
    return false
  end

  unless docker_available?
    puts red("Docker is not available. Please install and start Docker.")
    return false
  end

  parent_dir = File.expand_path("../..", SCRIPT_DIR)
  src_dir_name = File.join(File.basename(File.dirname(SCRIPT_DIR)), File.basename(SCRIPT_DIR))
  sdk_path = "/build/#{SDK_DIR_NAME}"

  build_script = <<~BASH
    set -e
    apt-get update > /dev/null 2>&1 && apt-get install -y build-essential cmake g++ #{extra_packages} > /dev/null 2>&1
    cd /build/#{src_dir_name}
    rm -rf build-linux-#{arch}
    mkdir -p build-linux-#{arch}
    cd build-linux-#{arch}
    cmake .. \\
      -DCMAKE_BUILD_TYPE=Release \\
      -DOLDUNREAL_SDK_PATH=#{sdk_path} \\
      #{extra_cmake_flags}
    make -j$(nproc)
    ls -la *.so 2>/dev/null || ls -la
  BASH

  docker_cmd = "docker run --rm " \
               "--platform #{docker_platform} " \
               "-v \"#{parent_dir}\":/build " \
               "-w /build/#{src_dir_name} " \
               "ubuntu:22.04 " \
               "/bin/bash -c '#{build_script.gsub("'", "'\\''")}'"

  puts yellow("> Running Docker build for #{arch}...")
  success = system(docker_cmd)

  unless success
    puts red("Build failed for #{arch}")
    return false
  end

  # Copy the built .so file (use absolute paths)
  so_file = File.join(SCRIPT_DIR, "build-linux-#{arch}", "PackageHelper_#{PH_VERSION}.so")
  output_path = File.join(SCRIPT_DIR, OUTPUT_DIR, output_subdir)
  output_file = File.join(output_path, "PackageHelper_#{PH_VERSION}.so")

  puts yellow("> Looking for: #{so_file}")

  # Wait briefly for Docker to finish writing
  sleep(1)

  if File.exist?(so_file)
    FileUtils.mkdir_p(output_path)
    FileUtils.cp(so_file, output_file)
    puts green("> Copied to #{output_file}")
    puts green("> File size: #{File.size(output_file)} bytes")
    return true
  else
    puts red("> Output file not found: #{so_file}")
    build_dir = File.dirname(so_file)
    if Dir.exist?(build_dir)
      puts yellow("> Contents of #{build_dir}:")
      Dir.entries(build_dir).each { |f| puts "    #{f}" unless f.start_with?('.') }
    end
    return false
  end
end

def main
  target = ARGV[0]&.downcase || "all"

  header "PackageHelper Native Build Script"
  puts "Target: #{target}"
  puts "Platform: #{macos? ? 'macOS (using Docker)' : 'Linux'}"

  sdk_path = File.join(File.expand_path("../..", SCRIPT_DIR), SDK_DIR_NAME)
  unless Dir.exist?(sdk_path)
    puts red("SDK not found at: #{sdk_path}")
    exit 1
  end

  results = {}

  case target
  when "amd64", "x64", "64"
    results["amd64"] = build_linux_so("amd64")
  when "x86", "32", "i386"
    results["x86"] = build_linux_so("x86")
  when "arm64", "aarch64"
    results["arm64"] = build_linux_so("arm64")
  when "all"
    results["amd64"] = build_linux_so("amd64")
    results["x86"] = build_linux_so("x86")
    results["arm64"] = build_linux_so("arm64")
  else
    puts "Usage: ruby buildPackageHelperLinux.rb [amd64|x86|arm64|all]"
    exit 1
  end

  header "Build Summary"
  results.each do |arch, success|
    status = success ? green("SUCCESS") : red("FAILED")
    puts "  #{arch}: #{status}"
  end

  # Show output files
  puts ""
  puts "Output directory: #{File.join(SCRIPT_DIR, OUTPUT_DIR)}"
  ["System", "System64", "SystemARM64"].each do |subdir|
    so_path = File.join(SCRIPT_DIR, OUTPUT_DIR, subdir, "PackageHelper_#{PH_VERSION}.so")
    if File.exist?(so_path)
      puts green("  #{subdir}/PackageHelper_#{PH_VERSION}.so (#{File.size(so_path)} bytes)")
    end
  end

  all_success = results.values.all?
  exit(all_success ? 0 : 1)
end

main
