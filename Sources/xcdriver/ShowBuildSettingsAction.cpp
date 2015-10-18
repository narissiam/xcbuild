// Copyright 2013-present Facebook. All Rights Reserved.

#include <xcdriver/ShowBuildSettingsAction.h>
#include <xcdriver/Options.h>
#include <xcdriver/Action.h>

using xcdriver::ShowBuildSettingsAction;
using xcdriver::Options;
using libutil::FSUtil;

ShowBuildSettingsAction::
ShowBuildSettingsAction()
{
}

ShowBuildSettingsAction::
~ShowBuildSettingsAction()
{
}

int ShowBuildSettingsAction::
Run(Options const &options)
{
    if (!Action::VerifyBuildActions(options.actions())) {
        return -1;
    }

    std::unique_ptr<pbxbuild::BuildEnvironment> buildEnvironment = pbxbuild::BuildEnvironment::Default();
    if (buildEnvironment == nullptr) {
        fprintf(stderr, "error: couldn't create build environment\n");
        return -1;
    }

    std::unique_ptr<pbxbuild::WorkspaceContext> workspaceContext = Action::CreateWorkspace(options);
    if (workspaceContext == nullptr) {
        return -1;
    }

    std::vector<pbxsetting::Level> overrideLevels = Action::CreateOverrideLevels(options, buildEnvironment->baseEnvironment());

    std::unique_ptr<pbxbuild::BuildContext> buildContext = Action::CreateBuildContext(options, *workspaceContext, overrideLevels);
    if (buildContext == nullptr) {
        return -1;
    }

    pbxbuild::DependencyResolver resolver = pbxbuild::DependencyResolver(*buildEnvironment);
    pbxbuild::BuildGraph<pbxproj::PBX::Target::shared_ptr> graph;
    if (buildContext->scheme() != nullptr) {
        graph = resolver.resolveSchemeDependencies(*buildContext);
    } else if (workspaceContext->project() != nullptr) {
        graph = resolver.resolveLegacyDependencies(*buildContext, options.allTargets(), options.target());
    } else {
        fprintf(stderr, "error: scheme is required for workspace\n");
        return -1;
    }

    std::vector<pbxproj::PBX::Target::shared_ptr> targets = graph.ordered();

    for (pbxproj::PBX::Target::shared_ptr const &target : targets) {
        std::unique_ptr<pbxbuild::TargetEnvironment> targetEnvironment = buildContext->targetEnvironment(*buildEnvironment, target);
        if (targetEnvironment == nullptr) {
            fprintf(stderr, "error: couldn't create target environment\n");
            continue;
        }

        pbxsetting::Environment const &environment = targetEnvironment->environment();
        std::unordered_map<std::string, std::string> values = targetEnvironment->environment().computeValues(pbxsetting::Condition::Empty());
        std::map<std::string, std::string> orderedValues = std::map<std::string, std::string>(values.begin(), values.end());

        printf("Build settings for action %s and target %s:\n", buildContext->action().c_str(), target->name().c_str());
        for (auto const &value : orderedValues) {
            printf("    %s = %s\n", value.first.c_str(), value.second.c_str());
        }
        printf("\n");
    }

    return 0;
}