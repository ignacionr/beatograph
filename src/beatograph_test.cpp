#include <gtest/gtest.h>

// Include the header file for the module being tested
#include "cloud/metrics/metrics_parser.hpp"

TEST(metrics_parser_test, should_parse_help_line) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_help = [&](const std::string_view& name, const std::string_view& help) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(help, "Apt packages pending autoremoval.");
    parsed = true;
  };
  parser("# HELP apt_autoremove_pending Apt packages pending autoremoval.");
  ASSERT_TRUE(parsed);
}

TEST(metrics_parser_test, should_parse_type_line) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_type = [&](const std::string_view& name, const std::string_view& type) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(type, "gauge");
    parsed = true;
  };
  parser("# TYPE apt_autoremove_pending gauge");
  ASSERT_TRUE(parsed);
}

TEST(metrics_parser_test, should_parse_metric_line) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(value.labels.size(), 0);
    ASSERT_EQ(value.value, 42.0);
    parsed = true;
  };
  parser("apt_autoremove_pending 42.0");
  ASSERT_TRUE(parsed);
}

TEST(metrics_parser_test, should_parse_tags) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(value.labels.size(), 1);
    ASSERT_EQ(value.labels.at("label"), "value");
    ASSERT_EQ(value.value, 42.0);
    parsed = true;
  };
  parser("apt_autoremove_pending{label=\"value\"} 42.0");
  ASSERT_TRUE(parsed);
}

TEST(metrics_parser_test, should_parse_multiline) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  int count{0};
  parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
    if (name == "apt_autoremove_pending") {
      ASSERT_EQ(value.labels.size(), 0);
      ASSERT_EQ(value.value, 42.0);
    } else if (name == "apt_autoremove_pending2") {
      ASSERT_EQ(value.labels.size(), 1);
      ASSERT_EQ(value.labels.at("label"), "value");
      ASSERT_EQ(value.value, 43.0);
    }
    count++;
  };
  parser("apt_autoremove_pending 42.0\napt_autoremove_pending2{label=\"value\"} 43.0");
  ASSERT_EQ(count, 2);
}

TEST(metrics_parser_test, should_handle_expontential) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(value.labels.size(), 0);
    ASSERT_EQ(value.value, 42.0e-3);
    parsed = true;
  };
  parser("apt_autoremove_pending 42.0e-3");
  ASSERT_TRUE(parsed);
}

TEST(metrics_parser_test, should_handle_tags_with_spaces) {
  // Create an instance of the beatograph module
  metrics_parser parser;
  bool parsed{false};
  parser.metric_metric_value = [&](std::string_view name, const metric_value& value) {
    ASSERT_EQ(name, "apt_autoremove_pending");
    ASSERT_EQ(value.labels.size(), 1);
    ASSERT_EQ(value.labels.at("label"), "value with spaces");
    ASSERT_EQ(value.value, 42.0);
    parsed = true;
  };
  parser("apt_autoremove_pending{label=\"value with spaces\"} 42.0");
  ASSERT_TRUE(parsed);
}

// Entry point for running the tests
int main(int argc, char** argv) {
  // Initialize the testing framework
  ::testing::InitGoogleTest(&argc, argv);

  // Run all the tests
  return RUN_ALL_TESTS();
}