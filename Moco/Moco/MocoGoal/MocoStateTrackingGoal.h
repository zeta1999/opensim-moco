#ifndef MOCO_MOCOSTATETRACKINGGOAL_H
#define MOCO_MOCOSTATETRACKINGGOAL_H
/* -------------------------------------------------------------------------- *
 * OpenSim Moco: MocoStateTrackingGoal.h                                      *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Christopher Dembia                                              *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may    *
 * not use this file except in compliance with the License. You may obtain a  *
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0          *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 * -------------------------------------------------------------------------- */

#include "../Common/TableProcessor.h"
#include "../MocoWeightSet.h"
#include "MocoGoal.h"

#include <OpenSim/Common/GCVSplineSet.h>
#include <OpenSim/Common/TimeSeriesTable.h>

namespace OpenSim {

// TODO can we track generalized speeds too?
// TODO allow raising error to different powers (cubed).
// TODO allow a "deadband."

/// The squared difference between a state variable value and a reference
/// state variable value, summed over the state variables for which a
/// reference is provided, and integrated over the phase. This can be used to
/// track joint angles, activations, etc.
/// The reference can be provided as a file name to a STO or CSV file (or
/// other file types for which there is a FileAdapter), or programmatically
/// as a TimeSeriesTable. If columns for rotational coordinates are in degrees,
/// those columns will be converted to radians.
/// Tracking problems in direct collocation perform best when tracking smooth
/// data, so it is recommended to filter the data in the reference you provide
/// to the cost.
/// @ingroup mocogoal
class OSIMMOCO_API MocoStateTrackingGoal : public MocoGoal {
    OpenSim_DECLARE_CONCRETE_OBJECT(MocoStateTrackingGoal, MocoGoal);

public:
    MocoStateTrackingGoal() { constructProperties(); }
    MocoStateTrackingGoal(std::string name) : MocoGoal(std::move(name)) {
        constructProperties();
    }
    MocoStateTrackingGoal(std::string name, double weight)
            : MocoGoal(std::move(name), weight) {
        constructProperties();
    }
    /// Provide a table containing reference values for the
    /// states you want to track. Each column label must be the path of a state
    /// variable, e.g., `knee/flexion/value`.
    /// The table is not loaded until the MocoProblem is initialized.
    void setReference(TableProcessor ref) {
        set_reference(std::move(ref));
    }

    /// Set the weight for an individual state variable. If a weight is
    /// already set for the requested state, then the provided weight
    /// replaces the previous weight. An exception is thrown if a weight
    /// for an unknown state is provided.
    void setWeightForState(const std::string& stateName, const double& weight) {
        if (get_state_weights().contains(stateName)) {
            upd_state_weights().get(stateName).setWeight(weight);
        } else {
            upd_state_weights().cloneAndAppend({stateName, weight});
        }
    }

    /// Provide a MocoWeightSet to weight the state variables in the cost.
    /// Replaces the weight set if it already exists.
    void setWeightSet(const MocoWeightSet& weightSet) {
        upd_state_weights() = weightSet;
    }

    /// If no reference has been provided, this returns an empty processor.
    const TableProcessor& getReference() const { return get_reference(); }

    /// Specify whether or not extra columns in the reference are allowed.
    /// If set true, the extra references will be ignored by the cost.
    /// If false, extra reference will cause an Exception to be raised.
    void setAllowUnusedReferences(bool tf) { set_allow_unused_references(tf); }

    /// Use the range, or the distance between the maximum and minimum value, of 
    /// each reference quantity to scale the weight for the associated tracking 
    /// error in the cost. The scale is computed by the inverse of the range, 
    /// so a reference quantity that changes less across the trajectory has a 
    /// larger weight. Each reference has a default weight of 1, so this flag
    /// works even if no user weights have be set. This may be useful when 
    /// tracking quantities with different units, which may have tracking errors 
    /// with different magnitudes.
    void setScaleWeightsWithRange(bool tf) { set_scale_weights_with_range(tf); }

protected:
    // TODO check that the reference covers the entire possible time range.
    void initializeOnModelImpl(const Model&) const override;
    void calcIntegrandImpl(
            const SimTK::State& state, double& integrand) const override;
    void calcGoalImpl(
            const GoalInput& input, SimTK::Vector& cost) const override {
        cost[0] = input.integral;
    }
    void printDescriptionImpl(std::ostream& stream = std::cout) const override;

private:
    OpenSim_DECLARE_PROPERTY(reference, TableProcessor,
            "Trajectories of states "
            "(coordinates, speeds, activation, etc.) to track. Column labels "
            "should be state variable paths, e.g., 'knee/flexion/value'");

    OpenSim_DECLARE_PROPERTY(allow_unused_references, bool,
            "Flag to determine whether or not references contained in the "
            "reference_file are allowed to be ignored by the cost.");

    OpenSim_DECLARE_PROPERTY(state_weights, MocoWeightSet,
            "Set of weight objects to weight the tracking of individual "
            "state variables in the cost.");

    OpenSim_DECLARE_PROPERTY(scale_weights_with_range, bool, 
            "Use the range, or the distance between the maximum and minimum "
            "value, of each reference quantity to scale the weight "
            "for the associated tracking error in the cost. The scale is "
            "computed by the inverse of the range, so a reference quantity "
            "that changes less across the trajectory has a larger weight. ");

    void constructProperties() {
        constructProperty_reference(TableProcessor());
        constructProperty_allow_unused_references(false);
        constructProperty_state_weights(MocoWeightSet());
        constructProperty_scale_weights_with_range(false);
    }

    mutable GCVSplineSet m_refsplines;
    /// The indices in Y corresponding to the provided reference coordinates.
    mutable std::vector<int> m_sysYIndices;
    mutable std::vector<double> m_state_weights;
    mutable std::vector<std::string> m_control_names;
};

} // namespace OpenSim

#endif // MOCO_MOCOSTATETRACKINGGOAL_H
