// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterToolset.h"

#include "Kismet/KismetSystemLibrary.h"

#include "Asset/TheFolderQuarantine.h"
#include "Graph/TheDependencyClosure.h"
#include "Graph/TheRootPartition.h"
#include "Graph/TheTextReferenceScan.h"
#include "Placement/ThePlacementResolver.h"
#include "Placement/TheStructureLint.h"
#include "TheQuartermasterSettings.h"

namespace TheQuartermasterToolset
{
/**
 * The taxonomy every placement tool answers from. A failure here is raised rather than returned:
 * an answer computed without the project's own rules would be indistinguishable from a real one.
 */
bool LoadPlacement(FThePlacementResolver& OutResolver)
{
    FString Error;
    if(FThePlacementResolver::LoadProjectConfig(OutResolver, Error))
    {
        return true;
    }
    UKismetSystemLibrary::RaiseScriptError(Error);
    return false;
}

bool LoadLint(FTheLintConfig& OutConfig)
{
    const FString ConfigPath = UTheQuartermasterSettings::ResolvedPlacementConfigPath();
    if(ConfigPath.IsEmpty())
    {
        UKismetSystemLibrary::RaiseScriptError(
            TEXT("no taxonomy: set PlacementConfigPath in the project's Editor settings"));
        return false;
    }

    FString Error;
    if(FTheStructureLint::LoadConfig(ConfigPath, OutConfig, Error))
    {
        return true;
    }
    UKismetSystemLibrary::RaiseScriptError(Error);
    return false;
}

/** "name=value" entries into facts. A malformed entry is refused, not skipped. */
bool ParseFacts(const TArray<FString>& Entries, TMap<FString, FString>& OutValues)
{
    for(const FString& Entry : Entries)
    {
        FString Key;
        FString Value;
        if(!Entry.Split(TEXT("="), &Key, &Value) || Key.TrimStartAndEnd().IsEmpty())
        {
            UKismetSystemLibrary::RaiseScriptError(
                FString::Printf(TEXT("fact '%s' is not in the form name=value"), *Entry));
            return false;
        }
        OutValues.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
    }
    return true;
}

FTheQuarantineReport Report(const FString& Text, bool bNeedsRestart)
{
    FTheQuarantineReport Result;
    Result.Report = Text;
    Result.bNeedsRestart = bNeedsRestart;
    return Result;
}
}

FTheStructureDescription UTheQuartermasterToolset::DescribeStructure()
{
    FTheStructureDescription Description;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(Resolver))
    {
        return Description;
    }

    const FThePlacementStructure Structure = Resolver.Describe();
    Description.ContentRoot = Structure.ContentRoot;
    Description.Contexts = Structure.Contexts;
    Description.PrefixKinds = Structure.PrefixKinds;
    Description.KindDirectories = Structure.KindDirectories;

    Description.Rules.Reserve(Structure.Rules.Num());
    for(const FThePlacementRuleInfo& Rule : Structure.Rules)
    {
        FTheStructureRule Entry;
        Entry.Context = Rule.Context;
        Entry.FolderTemplate = Rule.FolderTemplate;
        Entry.RequiredFacts = Rule.Requires;
        Entry.bRequiresNamePrefix = Rule.bRequiresPrefix;
        Description.Rules.Add(MoveTemp(Entry));
    }
    return Description;
}

FThePlacementAnswer UTheQuartermasterToolset::PlaceAsset(const FString& AssetName, const FString& Context, const TArray<FString>& Facts)
{
    FThePlacementAnswer Answer;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(Resolver))
    {
        return Answer;
    }

    FThePlacementFacts Request;
    Request.Name = AssetName;
    Request.Context = Context;
    if(!TheQuartermasterToolset::ParseFacts(Facts, Request.Values))
    {
        return Answer;
    }

    const FThePlacementResult Result = Resolver.Resolve(Request);
    Answer.PackagePath = Result.PackagePath;
    Answer.Folder = Result.Folder;
    Answer.NamePrefix = Result.Prefix;
    Answer.Kind = Result.Kind;
    Answer.MissingFacts = Result.MissingFacts;
    Answer.Reason = Result.Error;

    switch(Result.Outcome)
    {
    case EThePlacementOutcome::Placed:
        Answer.Outcome = TEXT("placed");
        break;
    case EThePlacementOutcome::NeedsFacts:
        Answer.Outcome = TEXT("needs-facts");
        break;
    default:
        Answer.Outcome = TEXT("rejected");
        break;
    }
    return Answer;
}

FThePathAccount UTheQuartermasterToolset::ExplainPath(const FString& PackagePath)
{
    FThePathAccount Account;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(Resolver))
    {
        return Account;
    }

    const FThePathExplanation Explanation = Resolver.Explain(PackagePath);
    Account.Context = Explanation.Context;
    Account.FolderTemplate = Explanation.FolderTemplate;
    Account.Folder = Explanation.Folder;
    Account.AssetName = Explanation.Name;
    Account.NamePrefix = Explanation.Prefix;
    Account.Kind = Explanation.Kind;
    Account.Facts = Explanation.Facts;
    Account.CandidateContexts = Explanation.CandidateContexts;
    Account.Reason = Explanation.Error;

    switch(Explanation.Outcome)
    {
    case ETheExplanationOutcome::Matched:
        Account.Outcome = TEXT("matched");
        break;
    case ETheExplanationOutcome::Ambiguous:
        Account.Outcome = TEXT("ambiguous");
        break;
    default:
        Account.Outcome = TEXT("unmatched");
        break;
    }
    return Account;
}

FTheLintReport UTheQuartermasterToolset::LintStructure(int32 MaxFindings)
{
    FTheLintReport Report;

    FTheLintConfig Config;
    if(!TheQuartermasterToolset::LoadLint(Config))
    {
        return Report;
    }

    FTheLintResult Result;
    FString Error;
    if(!FTheStructureLint::Run(Config, Result, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Report;
    }

    // The counts stay complete even when the list is cut: a truncated list that also truncated the
    // totals would read as a cleaner project than it is.
    Report.PackagesChecked = Result.PackagesChecked;
    Report.Errors = Result.CountOf(ETheLintSeverity::Error);
    Report.Warnings = Result.CountOf(ETheLintSeverity::Warning);

    const int32 Limit = MaxFindings > 0 ? FMath::Min(MaxFindings, Result.Findings.Num()) : Result.Findings.Num();
    Report.Omitted = Result.Findings.Num() - Limit;
    Report.Findings.Reserve(Limit);
    for(int32 Index = 0; Index < Limit; ++Index)
    {
        const FTheLintFinding& Finding = Result.Findings[Index];
        FTheLintFindingReport Entry;
        Entry.Rule = Finding.Rule;
        Entry.Package = Finding.Package;
        Entry.Detail = Finding.Detail;
        Entry.Severity = Finding.Severity == ETheLintSeverity::Error ? TEXT("error") : TEXT("warning");
        Report.Findings.Add(MoveTemp(Entry));
    }
    return Report;
}

FTheRootSurveyReport UTheQuartermasterToolset::SurveyRoots(const TArray<FString>& ExcludeRoots)
{
    FTheRootSurveyReport Survey;

    FTheRootPartitionResult Result;
    FString Error;
    if(!FTheRootPartition::Survey(ExcludeRoots, Result, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Survey;
    }

    Survey.TotalPackages = Result.TotalPackages;
    Survey.TotalExternalActors = Result.TotalExternalActors;
    Survey.TotalBytes = Result.TotalBytes;
    Survey.Roots.Reserve(Result.Roots.Num());
    for(const FTheRootProfile& Profile : Result.Roots)
    {
        FTheRootReport Entry;
        Entry.Root = Profile.Root;
        Entry.Packages = Profile.Packages;
        Entry.Bytes = Profile.Bytes;
        Entry.InboundReferencers = Profile.InboundReferencers;
        Entry.OutboundDependencies = Profile.OutboundDependencies;
        Entry.ExternalActors = Profile.ExternalActors;
        Entry.bIsIsland = Profile.bIsIsland;
        Survey.Roots.Add(MoveTemp(Entry));
    }
    return Survey;
}

FTheClosureReport UTheQuartermasterToolset::SolveClosure(const TArray<FString>& Roots, bool bIncludeTextReferences, const FString& ReportFilePath)
{
    FTheClosureReport Report;

    FTheClosureRequest Request;
    Request.Roots = Roots;
    Request.bIncludeTextReferences = bIncludeTextReferences;

    FTheClosureResult Result;
    FString Error;
    if(!FTheDependencyClosure::Solve(Request, Result, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Report;
    }

    Report.Candidates = Result.Candidates.Num();
    Report.Safe = Result.Safe.Num();
    Report.Held = Result.Held.Num();
    Report.FixpointRounds = Result.FixpointRounds;
    Report.ExternalActorsExcluded = Result.ExternalActorsExcluded;
    Report.HoldingRoots.Reserve(Result.HoldingRoots.Num());
    for(const TPair<FString, int32>& Pair : Result.HoldingRoots)
    {
        FTheHoldingRoot Entry;
        Entry.Root = Pair.Key;
        Entry.Holders = Pair.Value;
        Report.HoldingRoots.Add(MoveTemp(Entry));
    }

    if(!ReportFilePath.IsEmpty())
    {
        if(!FTheDependencyClosure::WriteReport(Result, ReportFilePath, Error))
        {
            UKismetSystemLibrary::RaiseScriptError(Error);
            return Report;
        }
        Report.ReportPath = ReportFilePath;
    }
    return Report;
}

FTheTextScanReport UTheQuartermasterToolset::ScanTextReferences(int32 MaxUnresolved)
{
    FTheTextScanReport Report;

    FTheTextScanResult Result;
    FString Error;
    if(!FTheTextReferenceScan::Scan(Result, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return Report;
    }

    Report.FilesScanned = Result.FilesScanned;
    Report.TokensSeen = Result.TokensSeen;
    Report.ReferencedPackages = Result.ReferencedPackages.Num();

    const int32 Total = Result.UnresolvedPaths.Num();
    const int32 Limit = MaxUnresolved > 0 ? FMath::Min(MaxUnresolved, Total) : Total;
    Report.Omitted = Total - Limit;
    Report.UnresolvedPaths.Reserve(Limit);
    for(const TPair<FString, TArray<FString>>& Pair : Result.UnresolvedPaths)
    {
        if(Report.UnresolvedPaths.Num() >= Limit)
        {
            break;
        }
        FTheUnresolvedPath Entry;
        Entry.Path = Pair.Key;
        Entry.Files = Pair.Value;
        Report.UnresolvedPaths.Add(MoveTemp(Entry));
    }
    return Report;
}

FTheQuarantineReport UTheQuartermasterToolset::QuarantineFolder(const FString& SourceFolder)
{
    FString Text;
    FString Error;
    bool bNeedsRestart = false;
    if(!FTheFolderQuarantine::MoveToQuarantine(SourceFolder, Text, bNeedsRestart, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(Text, bNeedsRestart);
}

FTheQuarantineReport UTheQuartermasterToolset::RestoreQuarantinedFolder(const FString& Folder)
{
    FString Text;
    FString Error;
    bool bNeedsRestart = false;
    if(!FTheFolderQuarantine::RestoreFromQuarantine(Folder, Text, bNeedsRestart, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(Text, bNeedsRestart);
}

FTheQuarantineReport UTheQuartermasterToolset::DeleteQuarantinedFolder(const FString& Folder)
{
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::DeleteFromQuarantine(Folder, Text, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(Text, /*bNeedsRestart*/ false);
}

FTheQuarantineReport UTheQuartermasterToolset::VerifyQuarantined(const FString& SourceFolder)
{
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::VerifyQuarantined(SourceFolder, Text, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(Text, /*bNeedsRestart*/ false);
}

FTheQuarantineReport UTheQuartermasterToolset::VerifyRestored(const FString& SourceFolder)
{
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::VerifyRestoredFromQuarantine(SourceFolder, Text, Error))
    {
        UKismetSystemLibrary::RaiseScriptError(Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(Text, /*bNeedsRestart*/ false);
}
