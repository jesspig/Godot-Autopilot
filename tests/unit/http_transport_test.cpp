#include "core/http_parse.hpp"
#include "core/http_transport.hpp"
#include "core/http_transport_ext.hpp"
#include "core/port_resolver.hpp"
#include "core/port_resolver_ext.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

using godot_autopilot::HttpRequest;
using godot_autopilot::HttpResponse;
using godot_autopilot::HttpRouteHandler;
using godot_autopilot::HttpRouteTable;
using godot_autopilot::PortAssignment;
using godot_autopilot::PortResolver;
using godot_autopilot::PortSource;

namespace {

constexpr int kCandidateBase = 20000;
constexpr int kCandidateLimit = 29999;

int candidate_count() { return kCandidateLimit - kCandidateBase + 1; }

} // namespace

TEST(HttpRouteTableTest, MatchesRegisteredPrefix) {
  HttpRouteTable table;
  bool invoked = false;
  table.add("GET", "/api/status",
            [&](const HttpRequest &, HttpResponse &response) {
              invoked = true;
              response.body = "status";
            });

  const HttpRouteHandler *handler = table.find("GET", "/api/status");
  ASSERT_NE(handler, nullptr);
  HttpRequest request;
  HttpResponse response;
  (*handler)(request, response);
  EXPECT_TRUE(invoked);
  EXPECT_EQ(response.body, "status");
}

TEST(HttpRouteTableTest, MissesUnknownPath) {
  HttpRouteTable table;
  table.add("GET", "/api/status",
            [](const HttpRequest &, HttpResponse &) {});
  EXPECT_EQ(table.find("GET", "/api/other"), nullptr);
}

TEST(HttpRouteTableTest, MissesOtherMethod) {
  HttpRouteTable table;
  table.add("GET", "/api/status",
            [](const HttpRequest &, HttpResponse &) {});
  EXPECT_EQ(table.find("POST", "/api/status"), nullptr);
}

TEST(HttpRouteTableTest, PrefersLongestPrefix) {
  HttpRouteTable table;
  table.add("GET", "/api",
            [](const HttpRequest &, HttpResponse &response) {
              response.body = "short";
            });
  table.add("GET", "/api/v2",
            [](const HttpRequest &, HttpResponse &response) {
              response.body = "long";
            });

  const HttpRouteHandler *handler = table.find("GET", "/api/v2/status");
  ASSERT_NE(handler, nullptr);
  HttpRequest request;
  HttpResponse response;
  (*handler)(request, response);
  EXPECT_EQ(response.body, "long");
}

TEST(HttpRouteTableTest, MethodMatchIsCaseInsensitive) {
  HttpRouteTable table;
  table.add("get", "/api/status",
            [](const HttpRequest &, HttpResponse &) {});
  EXPECT_NE(table.find("GET", "/api/status"), nullptr);
  EXPECT_NE(table.find("get", "/api/status"), nullptr);
}

TEST(HttpRouteTableTest, ReplacesHandlerOnDuplicateRoute) {
  HttpRouteTable table;
  table.add("GET", "/api/status",
            [](const HttpRequest &, HttpResponse &response) {
              response.body = "first";
            });
  table.add("GET", "/api/status",
            [](const HttpRequest &, HttpResponse &response) {
              response.body = "second";
            });

  const HttpRouteHandler *handler = table.find("GET", "/api/status");
  ASSERT_NE(handler, nullptr);
  HttpRequest request;
  HttpResponse response;
  (*handler)(request, response);
  EXPECT_EQ(response.body, "second");
}

TEST(HttpParseTest, ParsesRequestLine) {
  godot_autopilot::http_parse::RequestLine line;
  ASSERT_TRUE(godot_autopilot::http_parse::parse_request_line(
      "GET /api/status?verbose=1 HTTP/1.1", line));
  EXPECT_TRUE(line.ok);
  EXPECT_EQ(line.method, "GET");
  EXPECT_EQ(line.target, "/api/status?verbose=1");
  EXPECT_EQ(line.path, "/api/status");
  EXPECT_EQ(line.query, "verbose=1");
  EXPECT_EQ(line.version, "HTTP/1.1");
  ASSERT_EQ(line.query_params.size(), 1u);
  EXPECT_EQ(line.query_params.at("verbose"), "1");
}

TEST(HttpParseTest, RejectsMalformedRequestLine) {
  godot_autopilot::http_parse::RequestLine line;
  EXPECT_FALSE(godot_autopilot::http_parse::parse_request_line(
      "GET /api/status", line));
  EXPECT_FALSE(line.ok);
  EXPECT_FALSE(line.error.empty());
  EXPECT_FALSE(godot_autopilot::http_parse::parse_request_line(
      "not a request line at all", line));
}

TEST(HttpParseTest, RejectsMissingMethod) {
  godot_autopilot::http_parse::RequestLine line;
  EXPECT_FALSE(godot_autopilot::http_parse::parse_request_line(
      " /api/status HTTP/1.1", line));
  EXPECT_FALSE(line.error.empty());
}

TEST(HttpParseTest, ParsesHeaderBlock) {
  const std::string block =
      "Host: 127.0.0.1:9527\r\nContent-Length: 5\r\n"
      "Content-Type: application/json";
  godot_autopilot::http_parse::HeaderBlock headers;
  ASSERT_TRUE(godot_autopilot::http_parse::parse_headers(block, headers));
  EXPECT_TRUE(headers.ok);
  EXPECT_EQ(headers.headers.at("host"), "127.0.0.1:9527");
  EXPECT_EQ(headers.headers.at("content-length"), "5");
  EXPECT_EQ(headers.headers.at("content-type"), "application/json");
}

TEST(HttpParseTest, MissingContentLengthIsZero) {
  godot_autopilot::http_parse::HeaderBlock headers;
  ASSERT_TRUE(godot_autopilot::http_parse::parse_headers("Host: x", headers));
  size_t content_length = 99;
  ASSERT_TRUE(
      godot_autopilot::http_parse::parse_content_length(headers, content_length));
  EXPECT_EQ(content_length, 0u);
}

TEST(HttpParseTest, RejectsOversizedHeaderBlock) {
  const std::string block(godot_autopilot::http_parse::kMaxHeaderBlockBytes + 1,
                          'a');
  godot_autopilot::http_parse::HeaderBlock headers;
  EXPECT_FALSE(godot_autopilot::http_parse::parse_headers(block, headers));
  EXPECT_FALSE(headers.error.empty());
}

TEST(HttpParseTest, RejectsHeaderLineWithoutColon) {
  godot_autopilot::http_parse::HeaderBlock headers;
  EXPECT_FALSE(
      godot_autopilot::http_parse::parse_headers("BrokenHeader\r\n", headers));
  EXPECT_FALSE(headers.error.empty());
}

TEST(HttpParseTest, RejectsInvalidContentLength) {
  godot_autopilot::http_parse::HeaderBlock headers;
  ASSERT_TRUE(
      godot_autopilot::http_parse::parse_headers("Content-Length: abc", headers));
  size_t content_length = 0;
  EXPECT_FALSE(
      godot_autopilot::http_parse::parse_content_length(headers, content_length));
}

TEST(HttpParseTest, AcceptsBodyWithinLimit) {
  EXPECT_EQ(godot_autopilot::http_parse::decide_body(
                godot_autopilot::http_parse::kMaxBodyBytes),
            godot_autopilot::http_parse::BodyDecision::kAccept);
}

TEST(HttpParseTest, RejectsBodyOverLimit) {
  EXPECT_EQ(godot_autopilot::http_parse::decide_body(
                godot_autopilot::http_parse::kMaxBodyBytes + 1),
            godot_autopilot::http_parse::BodyDecision::kTooLarge);
}

TEST(HttpParseTest, ParsesSingleQueryParam) {
  const std::unordered_map<std::string, std::string> params =
      godot_autopilot::http_parse::parse_query("a=b");
  ASSERT_EQ(params.size(), 1u);
  EXPECT_EQ(params.at("a"), "b");
}

TEST(HttpParseTest, ParsesMultipleQueryParams) {
  const std::unordered_map<std::string, std::string> params =
      godot_autopilot::http_parse::parse_query("a=1&b=2&c=3");
  EXPECT_EQ(params.size(), 3u);
  EXPECT_EQ(params.at("a"), "1");
  EXPECT_EQ(params.at("b"), "2");
  EXPECT_EQ(params.at("c"), "3");
}

TEST(HttpParseTest, ParsesEmptyQuery) {
  EXPECT_TRUE(godot_autopilot::http_parse::parse_query("").empty());
}

TEST(HttpParseTest, ParsesQueryParamWithEmptyValue) {
  const std::unordered_map<std::string, std::string> params =
      godot_autopilot::http_parse::parse_query("a=");
  ASSERT_EQ(params.size(), 1u);
  EXPECT_EQ(params.at("a"), "");
}

TEST(HttpParseTest, ExposesQueryParamsToRouteHandlers) {
  HttpRequest request;
  request.headers[godot_autopilot::http_parse::kQueryRawHeader] = "a=b";
  request.headers[std::string(godot_autopilot::http_parse::kQueryHeaderPrefix) +
                  "a"] = "b";
  const std::unordered_map<std::string, std::string> params =
      godot_autopilot::http_request_query_params(request);
  ASSERT_EQ(params.size(), 1u);
  EXPECT_EQ(params.at("a"), "b");
  EXPECT_EQ(godot_autopilot::http_request_query_string(request), "a=b");
}

TEST(PortResolverTest, CandidateSequenceStartsWithDefaultPort) {
  const std::vector<int> sequence = PortResolver::candidate_sequence(4242);
  ASSERT_FALSE(sequence.empty());
  EXPECT_EQ(sequence.front(), 9527);
}

TEST(PortResolverTest, CandidateSequenceStartsAtPidDerivedPort) {
  const int pid = 12345;
  const std::vector<int> sequence = PortResolver::candidate_sequence(pid);
  ASSERT_GE(sequence.size(), 2u);
  EXPECT_EQ(sequence[1], kCandidateBase + (pid % 10000));
  EXPECT_EQ(godot_autopilot::port_auto_candidate_start(pid),
            kCandidateBase + (pid % 10000));
}

TEST(PortResolverTest, CandidateSequenceCoversWholeRange) {
  const std::vector<int> sequence = PortResolver::candidate_sequence(7);
  EXPECT_EQ(sequence.size(), static_cast<size_t>(candidate_count()) + 1);
  const std::set<int> unique(sequence.begin() + 1, sequence.end());
  EXPECT_EQ(unique.size(), static_cast<size_t>(candidate_count()));
  for (size_t index = 1; index < sequence.size(); ++index) {
    EXPECT_GE(sequence[index], kCandidateBase);
    EXPECT_LE(sequence[index], kCandidateLimit);
  }
}

TEST(PortResolverTest, CandidateSequenceWrapsAroundLimit) {
  const std::vector<int> sequence = PortResolver::candidate_sequence(9999);
  ASSERT_GE(sequence.size(), 3u);
  EXPECT_EQ(sequence[1], kCandidateLimit);
  EXPECT_EQ(sequence[2], kCandidateBase);
}

TEST(PortResolverTest, ExplicitPortWinsOverPreferredPort) {
  const PortAssignment assignment = PortResolver::resolve(1234, 5678, 42);
  EXPECT_EQ(assignment.source, PortSource::kEnvExplicit);
  EXPECT_EQ(assignment.port, 1234);
  EXPECT_EQ(assignment.requested_port, 1234);
  EXPECT_TRUE(assignment.note.empty());
}

TEST(PortResolverTest, PreferredPortWinsOverAutoAssignment) {
  const PortAssignment assignment = PortResolver::resolve(0, 5678, 42);
  EXPECT_EQ(assignment.source, PortSource::kConfigExplicit);
  EXPECT_EQ(assignment.port, 5678);
  EXPECT_EQ(assignment.requested_port, 5678);
  EXPECT_TRUE(assignment.note.empty());
}

TEST(PortResolverTest, AutoAssignmentUsesCandidateSequenceHead) {
  const PortAssignment assignment = PortResolver::resolve(0, 0, 42);
  EXPECT_EQ(assignment.source, PortSource::kAutoAssigned);
  EXPECT_EQ(assignment.port, 9527);
  EXPECT_EQ(assignment.requested_port, 0);
  EXPECT_FALSE(assignment.note.empty());
}
