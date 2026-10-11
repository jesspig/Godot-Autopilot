#include <gtest/gtest.h>

#include <string>

static const char kAbstractGateSuffix[] =
    " is not instantiable (abstract or invalid global class script): ";
static const char kProbeFailedSuffix[] = " (precheck=false, is_abstract=false, new_probe_failed)";

struct GateOutcome {
  bool created = false;
  std::string error;
};

static GateOutcome decide_create_gate(const std::string &type, const std::string &script_path,
                                      bool can_instantiate, bool is_abstract,
                                      bool new_probe_ok) {
  GateOutcome outcome;
  if (can_instantiate) {
    if (new_probe_ok) {
      outcome.created = true;
      return outcome;
    }
    outcome.error = "failed to instantiate global class: " + type;
    return outcome;
  }
  if (is_abstract) {
    outcome.error = type + kAbstractGateSuffix + script_path;
    return outcome;
  }
  if (new_probe_ok) {
    outcome.created = true;
    return outcome;
  }
  outcome.error = type + kAbstractGateSuffix + script_path + kProbeFailedSuffix;
  return outcome;
}

struct ListTypesDecision {
  bool observed = false;
  bool observed_value = false;
  bool admitted = false;
};

static ListTypesDecision decide_list_types(bool script_null, const std::string &base_type,
                                           bool base_is_resource, bool is_abstract,
                                           bool can_instantiate) {
  ListTypesDecision decision;
  if (script_null) {
    return decision;
  }
  decision.observed = true;
  decision.observed_value = can_instantiate;
  if (base_type.empty() || !base_is_resource || is_abstract) {
    return decision;
  }
  decision.admitted = true;
  return decision;
}

TEST(ResourceGateTest, PrecheckPassWithWorkingProbeCreates) {
  const GateOutcome outcome =
      decide_create_gate("MyItem", "res://my_item.gd", true, false, true);
  EXPECT_TRUE(outcome.created);
  EXPECT_TRUE(outcome.error.empty());
}

TEST(ResourceGateTest, PrecheckPassWithFailingProbeReportsGlobalClass) {
  const GateOutcome outcome =
      decide_create_gate("MyItem", "res://my_item.gd", true, false, false);
  EXPECT_FALSE(outcome.created);
  EXPECT_EQ("failed to instantiate global class: MyItem", outcome.error);
}

TEST(ResourceGateTest, AbstractScriptRejectedWithoutProbe) {
  const GateOutcome outcome =
      decide_create_gate("MyBase", "res://my_base.gd", false, true, false);
  EXPECT_FALSE(outcome.created);
  EXPECT_EQ("MyBase is not instantiable (abstract or invalid global class script): "
            "res://my_base.gd",
            outcome.error);
}

TEST(ResourceGateTest, PrecheckMissWithWorkingProbeCreates) {
  const GateOutcome outcome =
      decide_create_gate("MyDotNet", "res://MyDotNet.cs", false, false, true);
  EXPECT_TRUE(outcome.created);
  EXPECT_TRUE(outcome.error.empty());
}

TEST(ResourceGateTest, PrecheckMissWithFailingProbeCarriesProbeMarker) {
  const GateOutcome outcome =
      decide_create_gate("MyDotNet", "res://MyDotNet.cs", false, false, false);
  EXPECT_FALSE(outcome.created);
  EXPECT_EQ("MyDotNet is not instantiable (abstract or invalid global class script): "
            "res://MyDotNet.cs (precheck=false, is_abstract=false, new_probe_failed)",
            outcome.error);
}

TEST(ResourceGateTest, InstantiableScriptListedAndObserved) {
  const ListTypesDecision decision = decide_list_types(false, "Resource", true, false, true);
  EXPECT_TRUE(decision.observed);
  EXPECT_TRUE(decision.observed_value);
  EXPECT_TRUE(decision.admitted);
}

TEST(ResourceGateTest, AbstractScriptObservedButNotListed) {
  const ListTypesDecision decision = decide_list_types(false, "Resource", true, true, false);
  EXPECT_TRUE(decision.observed);
  EXPECT_FALSE(decision.observed_value);
  EXPECT_FALSE(decision.admitted);
}

TEST(ResourceGateTest, ProbeCandidateObservedAndListed) {
  const ListTypesDecision decision =
      decide_list_types(false, "Resource", true, false, false);
  EXPECT_TRUE(decision.observed);
  EXPECT_FALSE(decision.observed_value);
  EXPECT_TRUE(decision.admitted);
}

TEST(ResourceGateTest, NonResourceBaseObservedButNotListed) {
  const ListTypesDecision decision = decide_list_types(false, "Node", false, false, true);
  EXPECT_TRUE(decision.observed);
  EXPECT_TRUE(decision.observed_value);
  EXPECT_FALSE(decision.admitted);
}

TEST(ResourceGateTest, NullScriptNeitherObservedNorListed) {
  const ListTypesDecision decision = decide_list_types(true, "", false, false, false);
  EXPECT_FALSE(decision.observed);
  EXPECT_FALSE(decision.admitted);
}
