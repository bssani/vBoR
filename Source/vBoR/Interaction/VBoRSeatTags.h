#pragma once

#include "CoreMinimal.h"

namespace VBoRSeats
{
	enum ESeat { Driver, Passenger, RearLeft, RearCenter, RearRight };

	struct FSeat
	{
		const TCHAR* Tag;
		const TCHAR* LegacyTag;
		const TCHAR* Name;
		const TCHAR* ActionLabel;
	};

	inline constexpr FSeat Table[] = {
		{TEXT("Car.DriverSgRP"), TEXT("Car.DriverView"), TEXT("Driver"), TEXT("Return to driver seat")},
		{TEXT("Car.PassengerSgRP"), TEXT("Car.PassengerView"), TEXT("Passenger"), TEXT("Move to passenger seat")},
		{TEXT("Car.RearLeftSgRP"), TEXT("Car.RearView"), TEXT("Rear left"), TEXT("Move to rear left seat")},
		{TEXT("Car.RearCenterSgRP"), nullptr, TEXT("Rear center"), TEXT("Move to rear center seat")},
		{TEXT("Car.RearRightSgRP"), nullptr, TEXT("Rear right"), TEXT("Move to rear right seat")}
	};

	inline const FSeat* Find(FName Tag)
	{
		for (const FSeat& Seat : Table)
		{
			if (Tag == Seat.Tag || (Seat.LegacyTag && Tag == Seat.LegacyTag)) { return &Seat; }
		}
		return nullptr;
	}

	inline bool Matches(const TArray<FName>& Tags, FName Tag)
	{
		const FSeat* Seat = Find(Tag);
		return Seat ? Tags.Contains(FName(Seat->Tag)) || (Seat->LegacyTag && Tags.Contains(FName(Seat->LegacyTag))) : Tags.Contains(Tag);
	}

	inline bool Has(const TArray<FName>& Tags, ESeat Seat) { return Matches(Tags, FName(Table[Seat].Tag)); }

	inline const FSeat* Find(const TArray<FName>& Tags)
	{
		for (const FSeat& Seat : Table)
		{
			if (Matches(Tags, FName(Seat.Tag))) { return &Seat; }
		}
		return nullptr;
	}
}
