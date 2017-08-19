#include <sc2api/sc2_api.h>

#include <iostream>

using namespace sc2;

class Bot : public Agent {
public:
    virtual void OnGameStart() final {
    }

    virtual void OnStep() final {
		TryBuildSupplyDepot();
		TryBuildRefinery();
		TryBuildBarracks();
    }

	virtual void OnBuildingConstructionCompleted(const Unit& unit) final {
		const ObservationInterface* observation = Observation();
		switch (unit.unit_type.ToType()) {
			
		}
	}

	virtual void OnUnitCreate(const Unit& unit) final {
	}

	virtual void OnUnitIdle(const Unit& unit) final {
		const ObservationInterface* observation = Observation();
		switch (unit.unit_type.ToType()) {
			case UNIT_TYPEID::TERRAN_COMMANDCENTER: {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_SCV);
				break;
			}
			case UNIT_TYPEID::TERRAN_BARRACKS: {
				Actions()->UnitCommand(unit, ABILITY_ID::TRAIN_MARINE);
				break;
			}
			case UNIT_TYPEID::TERRAN_REFINERY: {
				int units_added = 0;
				Units units = observation->GetUnits(Unit::Alliance::Self);
				for (const auto& scv_unit : units) {
					if (units_added == 2)
						break;

					if (scv_unit.unit_type == UNIT_TYPEID::TERRAN_SCV) {
						Actions()->UnitCommand(scv_unit, ABILITY_ID::SMART, unit.tag);
						++units_added;
					}
				}
				break;
			}
			case UNIT_TYPEID::TERRAN_SCV: {
				uint64_t mineral_target;
				if (!FindNearestMineralPatch(unit.pos, mineral_target)) {
					break;
				}
				Actions()->UnitCommand(unit, ABILITY_ID::SMART, mineral_target);
				break;
			}

			case UNIT_TYPEID::TERRAN_MARINE: {
				if (CountUnitType(UNIT_TYPEID::TERRAN_MARINE) > 20 && !sentRush) {
					sentRush = 1;
					botRush();
					break;
				}
				break;
			}
			
			default: {
				break;
			}
		}
	}

	bool TryBuildStructure(ABILITY_ID ability_type_for_structure, UNIT_TYPEID unit_type = UNIT_TYPEID::TERRAN_SCV) {
		const ObservationInterface* observation = Observation();

		// If a unit already is building a supply structure of this type, do nothing.
		// Also get an scv to build the structure.
		Unit unit_to_build;
		Units units = observation->GetUnits(Unit::Alliance::Self);
		for (const auto& unit : units) {
			if (unit.unit_type == unit_type) {
				unit_to_build = unit;
			}
		}

		// Specialized build instructions for Vespene refineries
		if (ability_type_for_structure == ABILITY_ID::BUILD_REFINERY) {
			uint64_t vespene_target;
			FindNearestVespeneGeyser(unit_to_build.pos, vespene_target);
			Actions()->UnitCommand(unit_to_build, ability_type_for_structure, vespene_target);
			return true;
		}

		float rx = GetRandomScalar();
		float ry = GetRandomScalar();

		Actions()->UnitCommand(unit_to_build,
			ability_type_for_structure,
			Point2D(unit_to_build.pos.x + rx * 15.0f, unit_to_build.pos.y + ry * 15.0f));

		return true;
	}

	// Sends all active Marines on a scout mission, aka rush the enemy's base
	void botRush() {
		const ObservationInterface* observation = Observation();
		const GameInfo& game_info = Observation()->GetGameInfo();
		Units units = observation->GetUnits(Unit::Alliance::Self);
		for (const auto& unit : units) {
			if (unit.unit_type == UNIT_TYPEID::TERRAN_MARINE) {
				Actions()->UnitCommand(unit, ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
			}
		}
	}

	bool TryBuildSupplyDepot() {
		const ObservationInterface* observation = Observation();

		// If we are not supply capped, don't build a supply depot.
		if (observation->GetFoodUsed() <= observation->GetFoodCap() - 5)
			return false;

		// If there are not enough minerals, don't build a supply depot
		if (observation->GetMinerals() <= 100)
			return false;

		// Try and build a depot. Find a random SCV and give it the order.
		return TryBuildStructure(ABILITY_ID::BUILD_SUPPLYDEPOT);
	}
	
	bool TryBuildRefinery() {
		const ObservationInterface* observation = Observation();

		// Delay Start of production of Vespene
		if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
			return false;
		}

		// If we have 2 or more refineries, don't build a refinery
		if (CountUnitType(UNIT_TYPEID::TERRAN_REFINERY) >= 2 )
			return false;

		// If there are not enough minerals, don't build a refinery
		if (observation->GetMinerals() <= 75)
			return false;

		// Try and build a depot. Find a random SCV and give it the order.
		return TryBuildStructure(ABILITY_ID::BUILD_REFINERY);
	}

	bool TryBuildBarracks() {
		const ObservationInterface* observation = Observation();

		// Delay start of production of Barracks
		if (CountUnitType(UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
			return false;
		}

		// If there are already 3 Barracks, don't build a barracks
		if (CountUnitType(UNIT_TYPEID::TERRAN_BARRACKS) >= 3) {
			return false;
		}

		return TryBuildStructure(ABILITY_ID::BUILD_BARRACKS);
	}

	// Finds nearest Vespene Geyser, exactly the same as FindNearestMineralPatch
	// But with appropriate UNIT_TYPEID
	bool FindNearestVespeneGeyser(const Point2D& start, uint64_t& target) {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
		float distance = std::numeric_limits<float>::max();
		for (const auto& u : units) {
			if (u.unit_type == UNIT_TYPEID::NEUTRAL_VESPENEGEYSER) {
				float d = DistanceSquared2D(u.pos, start);
				if (d < distance) {
					distance = d;
					target = u.tag;
				}
			}
		}

		if (distance == std::numeric_limits<float>::max()) {
			return false;
		}
		return true;
	}

	bool FindNearestMineralPatch(const Point2D& start, uint64_t& target) {
		Units units = Observation()->GetUnits(Unit::Alliance::Neutral);
		float distance = std::numeric_limits<float>::max();
		for (const auto& u : units) {
			if (u.unit_type == UNIT_TYPEID::NEUTRAL_MINERALFIELD) {
				float d = DistanceSquared2D(u.pos, start);
				if (d < distance) {
					distance = d;
					target = u.tag;
				}
			}
		}

		if (distance == std::numeric_limits<float>::max()) {
			return false;
		}
		return true;
	}

	size_t CountUnitType(UNIT_TYPEID unit_type) {
		return Observation()->GetUnits(Unit::Alliance::Self, IsUnit(unit_type)).size();
	}
	private:
		bool sentRush = 0;
};

int main(int argc, char* argv[]) {
    Coordinator coordinator;
    coordinator.LoadSettings(argc, argv);

    Bot bot;
    coordinator.SetParticipants({
        CreateParticipant(Race::Terran, &bot),
        CreateComputer(Race::Zerg)
    });

    coordinator.LaunchStarcraft();
    coordinator.StartGame(sc2::kMapBelShirVestigeLE);

    while (coordinator.Update()) {
    }

    return 0;
}