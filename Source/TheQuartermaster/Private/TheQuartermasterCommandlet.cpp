// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheQuartermasterCommandlet.h"

#include "Asset/TheFolderQuarantine.h"
#include "Graph/TheDependencyClosure.h"
#include "Graph/TheRootPartition.h"
#include "Placement/ThePlacementResolver.h"
#include "Placement/TheStructureLint.h"
#include "TheQuartermasterModule.h"
#include "TheQuartermasterSettings.h"

namespace
{
void ReportSuccess(const TCHAR* Operation, const FString& Subject, const FString& Report)
{
    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_OK op=%s subject=%s"), Operation, *Subject);
    if(!Report.IsEmpty())
    {
        UE_LOG(LogTheQuartermaster, Display, TEXT("%s"), *Report);
    }
}

int32 ReportFailure(const TCHAR* Operation, const FString& Subject, const FString& Error)
{
    UE_LOG(LogTheQuartermaster, Error, TEXT("THEQM_FAIL op=%s subject=%s error=%s"), Operation, *Subject, *Error);
    return 2;
}

bool LoadPlacement(const TMap<FString, FString>& Arguments, FThePlacementResolver& OutResolver, FString& OutError)
{
    if(const FString* ConfigPath = Arguments.Find(TEXT("Config")))
    {
        if(!ConfigPath->IsEmpty())
        {
            return OutResolver.LoadConfig(*ConfigPath, OutError);
        }
    }
    return FThePlacementResolver::LoadProjectConfig(OutResolver, OutError);
}

FString JoinFacts(const TMap<FString, FString>& Facts)
{
    TArray<FString> Pairs;
    Pairs.Reserve(Facts.Num());
    for(const TPair<FString, FString>& Pair : Facts)
    {
        Pairs.Add(FString::Printf(TEXT("%s=%s"), *Pair.Key, *Pair.Value));
    }
    Pairs.Sort();
    return FString::Join(Pairs, TEXT(","));
}
}

int32 UTheQuartermasterCommandlet::Main(const FString& Params)
{
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> Arguments;
    ParseCommandLine(*Params, Tokens, Switches, Arguments);

    FString Report;
    FString Error;
    bool bNeedsRestart = false;

    UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_ROOT %s"), *FTheFolderQuarantine::QuarantineRoot());

    if(const FString* Folder = Arguments.Find(TEXT("Quarantine")))
    {
        if(!FTheFolderQuarantine::MoveToQuarantine(*Folder, Report, bNeedsRestart, Error))
        {
            return ReportFailure(TEXT("quarantine"), *Folder, Error);
        }
        ReportSuccess(TEXT("quarantine"), *Folder, Report);
        if(bNeedsRestart)
        {
            UE_LOG(LogTheQuartermaster, Warning, TEXT("THEQM_NEEDS_RESTART op=quarantine subject=%s"), **Folder);
        }
        return 0;
    }

    if(const FString* Folder = Arguments.Find(TEXT("Restore")))
    {
        if(!FTheFolderQuarantine::RestoreFromQuarantine(*Folder, Report, bNeedsRestart, Error))
        {
            return ReportFailure(TEXT("restore"), *Folder, Error);
        }
        ReportSuccess(TEXT("restore"), *Folder, Report);
        if(bNeedsRestart)
        {
            UE_LOG(LogTheQuartermaster, Warning, TEXT("THEQM_NEEDS_RESTART op=restore subject=%s"), **Folder);
        }
        return 0;
    }

    if(const FString* Folder = Arguments.Find(TEXT("DeleteQuarantined")))
    {
        if(!FTheFolderQuarantine::DeleteFromQuarantine(*Folder, Switches.Contains(TEXT("Force")), Report, Error))
        {
            return ReportFailure(TEXT("delete"), *Folder, Error);
        }
        ReportSuccess(TEXT("delete"), *Folder, Report);
        return 0;
    }

    if(const FString* Folder = Arguments.Find(TEXT("VerifyQuarantined")))
    {
        if(!FTheFolderQuarantine::VerifyQuarantined(*Folder, Report, Error))
        {
            return ReportFailure(TEXT("verify-quarantined"), *Folder, Error);
        }
        ReportSuccess(TEXT("verify-quarantined"), *Folder, Report);
        return 0;
    }

    if(Switches.Contains(TEXT("Lint")))
    {
        FString ConfigPath;
        if(const FString* Explicit = Arguments.Find(TEXT("Config")))
        {
            ConfigPath = *Explicit;
        }
        if(ConfigPath.IsEmpty())
        {
            ConfigPath = UTheQuartermasterSettings::ResolvedPlacementConfigPath();
        }
        if(ConfigPath.IsEmpty())
        {
            return ReportFailure(TEXT("lint"), TEXT("/Game"), TEXT("no taxonomy: pass -Config=<file> or set PlacementConfigPath in the project's Editor settings"));
        }

        FTheLintConfig Config;
        if(!FTheStructureLint::LoadConfig(ConfigPath, Config, Error))
        {
            return ReportFailure(TEXT("lint"), ConfigPath, Error);
        }

        FTheLintResult Result;
        if(!FTheStructureLint::Run(Config, Result, Error))
        {
            return ReportFailure(TEXT("lint"), ConfigPath, Error);
        }

        for(const FTheLintFinding& Finding : Result.Findings)
        {
            if(Finding.Severity == ETheLintSeverity::Error)
            {
                UE_LOG(LogTheQuartermaster, Error, TEXT("THEQM_LINT_FINDING severity=error rule=%s package=%s detail=%s"), *Finding.Rule, *Finding.Package, *Finding.Detail);
            }
            else
            {
                UE_LOG(LogTheQuartermaster, Warning, TEXT("THEQM_LINT_FINDING severity=warning rule=%s package=%s detail=%s"), *Finding.Rule, *Finding.Package, *Finding.Detail);
            }
        }

        const int32 Errors = Result.CountOf(ETheLintSeverity::Error);
        const int32 Warnings = Result.CountOf(ETheLintSeverity::Warning);

        if(Errors > 0)
        {
            UE_LOG(LogTheQuartermaster, Error, TEXT("THEQM_FAIL op=lint subject=%s error=%d structural error(s), %d warning(s) over %d packages"), *ConfigPath, Errors, Warnings, Result.PackagesChecked);
            return 3;
        }
        ReportSuccess(TEXT("lint"), ConfigPath, FString::Printf(TEXT("checked=%d errors=0 warnings=%d"), Result.PackagesChecked, Warnings));
        return 0;
    }

    if(Switches.Contains(TEXT("Partition")))
    {
        const FString* Out = Arguments.Find(TEXT("Out"));
        if(!Out || Out->IsEmpty())
        {
            return ReportFailure(TEXT("partition"), TEXT("/Game"), TEXT("-Out=<file> is required"));
        }

        TArray<FString> Exclude;
        if(const FString* Excluded = Arguments.Find(TEXT("Exclude")))
        {
            Excluded->ParseIntoArray(Exclude, TEXT(","), /*InCullEmpty*/ true);
        }

        FTheRootPartitionResult Result;
        if(!FTheRootPartition::Survey(Exclude, Result, Error))
        {
            return ReportFailure(TEXT("partition"), TEXT("/Game"), Error);
        }
        if(!FTheRootPartition::WriteReport(Result, *Out, Error))
        {
            return ReportFailure(TEXT("partition"), TEXT("/Game"), Error);
        }
        ReportSuccess(TEXT("partition"), TEXT("/Game"), FString::Printf(TEXT("roots=%d packages=%d external_actors=%d"), Result.Roots.Num(), Result.TotalPackages, Result.TotalExternalActors));
        return 0;
    }

    if(const FString* Roots = Arguments.Find(TEXT("Closure")))
    {
        const FString* Out = Arguments.Find(TEXT("Out"));
        if(!Out || Out->IsEmpty())
        {
            return ReportFailure(TEXT("closure"), *Roots, TEXT("-Out=<file> is required"));
        }

        FTheClosureRequest Request;
        Roots->ParseIntoArray(Request.Roots, TEXT(","), /*InCullEmpty*/ true);

        FTheClosureResult Result;
        if(!FTheDependencyClosure::Solve(Request, Result, Error))
        {
            return ReportFailure(TEXT("closure"), *Roots, Error);
        }
        if(!FTheDependencyClosure::WriteReport(Result, *Out, Error))
        {
            return ReportFailure(TEXT("closure"), *Roots, Error);
        }
        ReportSuccess(TEXT("closure"), *Roots, FString::Printf(TEXT("candidates=%d safe=%d held=%d rounds=%d"), Result.Candidates.Num(), Result.Safe.Num(), Result.Held.Num(), Result.FixpointRounds));
        return 0;
    }

    if(Switches.Contains(TEXT("Describe")))
    {
        FThePlacementResolver Resolver;
        if(!LoadPlacement(Arguments, Resolver, Error))
        {
            return ReportFailure(TEXT("describe"), TEXT("placement"), Error);
        }

        const FThePlacementStructure Structure = Resolver.Describe();
        UE_LOG(LogTheQuartermaster, Display, TEXT("THEQM_STRUCTURE content_root=%s contexts=%d prefixes=%d rules=%d"), *Structure.ContentRoot, Structure.Contexts.Num(), Structure.PrefixKinds.Num(), Structure.Rules.Num());
        for(const FThePlacementRuleInfo& Rule : Structure.Rules)
        {
            UE_LOG(LogTheQuartermaster,
                Display,
                TEXT("THEQM_RULE context=%s folder=%s requires=%s prefix=%s"),
                *Rule.Context,
                *Rule.FolderTemplate,
                Rule.Requires.IsEmpty() ? TEXT("-") : *FString::Join(Rule.Requires, TEXT(",")),
                Rule.bRequiresPrefix ? TEXT("required") : TEXT("none"));
        }
        ReportSuccess(TEXT("describe"), *Structure.ContentRoot, FString::Printf(TEXT("contexts=%d rules=%d"), Structure.Contexts.Num(), Structure.Rules.Num()));
        return 0;
    }

    if(const FString* PackagePath = Arguments.Find(TEXT("Explain")))
    {
        FThePlacementResolver Resolver;
        if(!LoadPlacement(Arguments, Resolver, Error))
        {
            return ReportFailure(TEXT("explain"), *PackagePath, Error);
        }

        const FThePathExplanation Explanation = Resolver.Explain(*PackagePath);
        switch(Explanation.Outcome)
        {
            case ETheExplanationOutcome::Matched:
                ReportSuccess(TEXT("explain"),
                    *PackagePath,
                    FString::Printf(TEXT("context=%s template=%s prefix=%s kind=%s facts=%s"),
                        *Explanation.Context,
                        *Explanation.FolderTemplate,
                        Explanation.Prefix.IsEmpty() ? TEXT("-") : *Explanation.Prefix,
                        Explanation.Kind.IsEmpty() ? TEXT("-") : *Explanation.Kind,
                        *JoinFacts(Explanation.Facts)));
                return 0;
            case ETheExplanationOutcome::Ambiguous:
                UE_LOG(LogTheQuartermaster, Warning, TEXT("THEQM_EXPLAIN_AMBIGUOUS subject=%s contexts=%s"), **PackagePath, *FString::Join(Explanation.CandidateContexts, TEXT(",")));
                return 4;
            default:
                UE_LOG(LogTheQuartermaster,
                    Warning,
                    TEXT("THEQM_EXPLAIN_UNMATCHED subject=%s near=%s error=%s"),
                    **PackagePath,
                    Explanation.CandidateContexts.IsEmpty() ? TEXT("-") : *FString::Join(Explanation.CandidateContexts, TEXT(",")),
                    *Explanation.Error);
                return 4;
        }
    }

    if(const FString* Name = Arguments.Find(TEXT("Place")))
    {
        FThePlacementResolver Resolver;
        if(!LoadPlacement(Arguments, Resolver, Error))
        {
            return ReportFailure(TEXT("place"), *Name, Error);
        }

        FThePlacementFacts Facts;
        Facts.Name = *Name;
        if(const FString* Context = Arguments.Find(TEXT("Context")))
        {
            Facts.Context = *Context;
        }

        if(const FString* Pairs = Arguments.Find(TEXT("Facts")))
        {
            TArray<FString> FactPairs;
            Pairs->ParseIntoArray(FactPairs, TEXT(","), /*InCullEmpty*/ true);
            for(const FString& Token : FactPairs)
            {
                FString Key;
                FString Value;
                if(Token.Split(TEXT("="), &Key, &Value))
                {
                    Facts.Values.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
                }
            }
        }

        const FThePlacementResult Result = Resolver.Resolve(Facts);
        switch(Result.Outcome)
        {
            case EThePlacementOutcome::Placed:
                ReportSuccess(TEXT("place"), *Name, FString::Printf(TEXT("path=%s folder=%s"), *Result.PackagePath, *Result.Folder));
                return 0;
            case EThePlacementOutcome::NeedsFacts:
                UE_LOG(LogTheQuartermaster, Warning, TEXT("THEQM_PLACE_NEEDS_FACTS subject=%s missing=%s"), **Name, *FString::Join(Result.MissingFacts, TEXT(",")));
                return 4;
            default:
                return ReportFailure(TEXT("place"), *Name, Result.Error);
        }
    }

    if(const FString* Folder = Arguments.Find(TEXT("VerifyRestored")))
    {
        if(!FTheFolderQuarantine::VerifyRestoredFromQuarantine(*Folder, Report, Error))
        {
            return ReportFailure(TEXT("verify-restored"), *Folder, Error);
        }
        ReportSuccess(TEXT("verify-restored"), *Folder, Report);
        return 0;
    }

    UE_LOG(LogTheQuartermaster, Error, TEXT("THEQM_FAIL op=none error=no operation given; expected one of -Quarantine= -Restore= -DeleteQuarantined= -VerifyQuarantined= -VerifyRestored= -Closure= -Explain= -Place= -Partition -Lint -Describe"));
    return 1;
}
