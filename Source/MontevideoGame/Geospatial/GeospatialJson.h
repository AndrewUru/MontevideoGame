#pragma once
#include "Geospatial/GeospatialBlockoutData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformTime.h"

namespace GeospatialJson
{
	/** Shared bounded local reader. Metrics separate disk IO, checksum and UTF decoding/JSON parsing. */
	inline bool Read(const FString& Root, const FString& Relative, const FString& Hash,
		TSharedPtr<FJsonObject>& Out, FString& Error, FGeospatialLoadMetrics* Metrics = nullptr)
	{
		const double Start=FPlatformTime::Seconds();
		FString Path=FPaths::ConvertRelativePathToFull(FPaths::Combine(Root,Relative));
		FPaths::CollapseRelativeDirectories(Path);
		if (!FPaths::IsRelative(Relative) || !FPaths::IsUnderDirectory(Path,FPaths::ConvertRelativePathToFull(Root)))
		{ Error=TEXT("Data path escapes project directory"); return false; }
		TArray<uint8> Bytes;
		const int64 Size=IFileManager::Get().FileSize(*Path);
		if (Size<0 || Size>25*1024*1024 || !FFileHelper::LoadFileToArray(Bytes,*Path))
		{ Error=TEXT("Cannot read bounded data file: ")+Relative; return false; }
		const double AfterRead=FPlatformTime::Seconds();
		if (!Hash.IsEmpty() && !FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num()).ToString().Equals(Hash,ESearchCase::IgnoreCase))
		{ Error=TEXT("Data checksum mismatch: ")+Relative; return false; }
		const double AfterHash=FPlatformTime::Seconds();
		FString Content;
		FFileHelper::BufferToString(Content,Bytes.GetData(),Bytes.Num());
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Content),Out) || !Out)
		{ Error=TEXT("Malformed JSON: ")+Relative; return false; }
		if (Metrics)
		{
			Metrics->ReadMilliseconds+=(AfterRead-Start)*1000;
			Metrics->ChecksumMilliseconds+=(AfterHash-AfterRead)*1000;
			Metrics->ParseMilliseconds+=(FPlatformTime::Seconds()-AfterHash)*1000;
		}
		return true;
	}
}
