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
void RaiseError(const TCHAR* Tool, const FString& Error)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("%s: failed - %s"), Tool, *Error);
    UKismetSystemLibrary::RaiseScriptError(Error);
}

bool LoadPlacement(const TCHAR* Tool, FThePlacementResolver& OutResolver)
{
    FString Error;
    if(FThePlacementResolver::LoadProjectConfig(OutResolver, Error))
    {
        UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("%s: placement config %s loaded"), Tool, *UTheQuartermasterSettings::ResolvedPlacementConfigPath());
        return true;
    }
    RaiseError(Tool, Error);
    return false;
}

bool LoadLint(const TCHAR* Tool, FTheLintConfig& OutConfig)
{
    const FString ConfigPath = UTheQuartermasterSettings::ResolvedPlacementConfigPath();
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("%s: lint config path '%s'"), Tool, *ConfigPath);
    if(ConfigPath.IsEmpty())
    {
        RaiseError(Tool, TEXT("no taxonomy: set PlacementConfigPath in the project's Editor settings"));
        return false;
    }

    FString Error;
    if(FTheStructureLint::LoadConfig(ConfigPath, OutConfig, Error))
    {
        return true;
    }
    RaiseError(Tool, Error);
    return false;
}

bool ParseFacts(const TCHAR* Tool, const TArray<FString>& Entries, TMap<FString, FString>& OutValues)
{
    for(const FString& Entry : Entries)
    {
        FString Key;
        FString Value;
        if(!Entry.Split(TEXT("="), &Key, &Value) || Key.TrimStartAndEnd().IsEmpty())
        {
            RaiseError(Tool, FString::Printf(TEXT("fact '%s' is not in the form name=value"), *Entry));
            return false;
        }
        OutValues.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
        UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("%s: fact %s = '%s'"), Tool, *Key.TrimStartAndEnd(), *Value.TrimStartAndEnd());
    }
    return true;
}

FTheQuarantineReport Report(const TCHAR* Tool, const FString& Text, bool bNeedsRestart)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("%s: ok, needs restart=%d - %s"), Tool, bNeedsRestart ? 1 : 0, *Text);
    FTheQuarantineReport Result;
    Result.Report = Text;
    Result.bNeedsRestart = bNeedsRestart;
    return Result;
}
}

FTheStructureDescription UTheQuartermasterToolset::DescribeStructure()
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("DescribeStructure: called"));
    FTheStructureDescription Description;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(TEXT("DescribeStructure"), Resolver))
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
    UE_LOG(LogTheQuartermasterToolset,
        Verbose,
        TEXT("DescribeStructure: content root %s, %d context(s), %d prefix(es), %d kind dir(s), %d rule(s)"),
        *Description.ContentRoot,
        Description.Contexts.Num(),
        Description.PrefixKinds.Num(),
        Description.KindDirectories.Num(),
        Description.Rules.Num());
    return Description;
}

FThePlacementAnswer UTheQuartermasterToolset::PlaceAsset(const FString& AssetName, const FString& Context, const TArray<FString>& Facts)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("PlaceAsset: name '%s', context '%s', %d fact(s) [%s]"), *AssetName, *Context, Facts.Num(), *FString::Join(Facts, TEXT(", ")));
    FThePlacementAnswer Answer;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(TEXT("PlaceAsset"), Resolver))
    {
        return Answer;
    }

    FThePlacementFacts Request;
    Request.Name = AssetName;
    Request.Context = Context;
    if(!TheQuartermasterToolset::ParseFacts(TEXT("PlaceAsset"), Facts, Request.Values))
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
    UE_LOG(LogTheQuartermasterToolset,
        Verbose,
        TEXT("PlaceAsset: %s - path '%s', folder '%s', prefix '%s', kind '%s', missing [%s], reason '%s'"),
        *Answer.Outcome,
        *Answer.PackagePath,
        *Answer.Folder,
        *Answer.NamePrefix,
        *Answer.Kind,
        *FString::Join(Answer.MissingFacts, TEXT(", ")),
        *Answer.Reason);
    return Answer;
}

FThePathAccount UTheQuartermasterToolset::ExplainPath(const FString& PackagePath)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("ExplainPath: '%s'"), *PackagePath);
    FThePathAccount Account;

    FThePlacementResolver Resolver;
    if(!TheQuartermasterToolset::LoadPlacement(TEXT("ExplainPath"), Resolver))
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
    UE_LOG(LogTheQuartermasterToolset,
        Verbose,
        TEXT("ExplainPath: %s - context '%s', template '%s', folder '%s', name '%s', %d fact(s), candidates [%s], reason '%s'"),
        *Account.Outcome,
        *Account.Context,
        *Account.FolderTemplate,
        *Account.Folder,
        *Account.AssetName,
        Account.Facts.Num(),
        *FString::Join(Account.CandidateContexts, TEXT(", ")),
        *Account.Reason);
    return Account;
}

FTheLintReport UTheQuartermasterToolset::LintStructure(int32 MaxFindings)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("LintStructure: MaxFindings=%d"), MaxFindings);
    FTheLintReport Report;

    FTheLintConfig Config;
    if(!TheQuartermasterToolset::LoadLint(TEXT("LintStructure"), Config))
    {
        return Report;
    }

    FTheLintResult Result;
    FString Error;
    if(!FTheStructureLint::Run(Config, Result, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("LintStructure"), Error);
        return Report;
    }

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
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("LintStructure: checked=%d errors=%d warnings=%d findings=%d returned=%d omitted=%d"), Report.PackagesChecked, Report.Errors, Report.Warnings, Result.Findings.Num(), Limit, Report.Omitted);
    return Report;
}

FTheRootSurveyReport UTheQuartermasterToolset::SurveyRoots(const TArray<FString>& ExcludeRoots)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("SurveyRoots: %d excluded root(s) [%s]"), ExcludeRoots.Num(), *FString::Join(ExcludeRoots, TEXT(", ")));
    FTheRootSurveyReport Survey;

    FTheRootPartitionResult Result;
    FString Error;
    if(!FTheRootPartition::Survey(ExcludeRoots, Result, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("SurveyRoots"), Error);
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
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("SurveyRoots: roots=%d packages=%d external_actors=%d bytes=%lld"), Survey.Roots.Num(), Survey.TotalPackages, Survey.TotalExternalActors, static_cast<long long>(Survey.TotalBytes));
    return Survey;
}

FTheClosureReport UTheQuartermasterToolset::SolveClosure(const TArray<FString>& Roots, bool bIncludeTextReferences, const FString& ReportFilePath)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("SolveClosure: %d root(s) [%s], text references=%d, report '%s'"), Roots.Num(), *FString::Join(Roots, TEXT(", ")), bIncludeTextReferences ? 1 : 0, *ReportFilePath);
    FTheClosureReport Report;

    FTheClosureRequest Request;
    Request.Roots = Roots;
    Request.bIncludeTextReferences = bIncludeTextReferences;

    FTheClosureResult Result;
    FString Error;
    if(!FTheDependencyClosure::Solve(Request, Result, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("SolveClosure"), Error);
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
    UE_LOG(LogTheQuartermasterToolset,
        Verbose,
        TEXT("SolveClosure: candidates=%d safe=%d held=%d rounds=%d external_actors_excluded=%d holding_roots=%d"),
        Report.Candidates,
        Report.Safe,
        Report.Held,
        Report.FixpointRounds,
        Report.ExternalActorsExcluded,
        Report.HoldingRoots.Num());

    if(!ReportFilePath.IsEmpty())
    {
        if(!FTheDependencyClosure::WriteReport(Result, ReportFilePath, Error))
        {
            TheQuartermasterToolset::RaiseError(TEXT("SolveClosure"), Error);
            return Report;
        }
        Report.ReportPath = ReportFilePath;
        UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("SolveClosure: report written to '%s'"), *Report.ReportPath);
    }
    return Report;
}

FTheTextScanReport UTheQuartermasterToolset::ScanTextReferences(int32 MaxUnresolved)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("ScanTextReferences: MaxUnresolved=%d"), MaxUnresolved);
    FTheTextScanReport Report;

    FTheTextScanResult Result;
    FString Error;
    if(!FTheTextReferenceScan::Scan(Result, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("ScanTextReferences"), Error);
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
    UE_LOG(LogTheQuartermasterToolset,
        Verbose,
        TEXT("ScanTextReferences: files=%d tokens=%d referenced=%d unresolved=%d returned=%d omitted=%d"),
        Report.FilesScanned,
        Report.TokensSeen,
        Report.ReferencedPackages,
        Total,
        Report.UnresolvedPaths.Num(),
        Report.Omitted);
    return Report;
}

FTheQuarantineReport UTheQuartermasterToolset::QuarantineFolder(const FString& SourceFolder)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("QuarantineFolder: '%s', quarantine root %s"), *SourceFolder, *FTheFolderQuarantine::QuarantineRoot());
    FString Text;
    FString Error;
    bool bNeedsRestart = false;
    if(!FTheFolderQuarantine::MoveToQuarantine(SourceFolder, Text, bNeedsRestart, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("QuarantineFolder"), Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(TEXT("QuarantineFolder"), Text, bNeedsRestart);
}

FTheQuarantineReport UTheQuartermasterToolset::RestoreQuarantinedFolder(const FString& Folder)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("RestoreQuarantinedFolder: '%s'"), *Folder);
    FString Text;
    FString Error;
    bool bNeedsRestart = false;
    if(!FTheFolderQuarantine::RestoreFromQuarantine(Folder, Text, bNeedsRestart, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("RestoreQuarantinedFolder"), Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(TEXT("RestoreQuarantinedFolder"), Text, bNeedsRestart);
}

FTheQuarantineReport UTheQuartermasterToolset::DeleteQuarantinedFolder(const FString& Folder, bool bForce)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("DeleteQuarantinedFolder: '%s', bForce=%d"), *Folder, bForce ? 1 : 0);
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::DeleteFromQuarantine(Folder, bForce, Text, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("DeleteQuarantinedFolder"), Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(TEXT("DeleteQuarantinedFolder"), Text, /*bNeedsRestart*/ false);
}

FTheQuarantineReport UTheQuartermasterToolset::VerifyQuarantined(const FString& SourceFolder)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("VerifyQuarantined: '%s'"), *SourceFolder);
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::VerifyQuarantined(SourceFolder, Text, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("VerifyQuarantined"), Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(TEXT("VerifyQuarantined"), Text, /*bNeedsRestart*/ false);
}

FTheQuarantineReport UTheQuartermasterToolset::VerifyRestored(const FString& SourceFolder)
{
    UE_LOG(LogTheQuartermasterToolset, Verbose, TEXT("VerifyRestored: '%s'"), *SourceFolder);
    FString Text;
    FString Error;
    if(!FTheFolderQuarantine::VerifyRestoredFromQuarantine(SourceFolder, Text, Error))
    {
        TheQuartermasterToolset::RaiseError(TEXT("VerifyRestored"), Error);
        return FTheQuarantineReport();
    }
    return TheQuartermasterToolset::Report(TEXT("VerifyRestored"), Text, /*bNeedsRestart*/ false);
}
