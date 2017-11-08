/* -------------------------------------------------------------------------- *
 * OpenSim Muscollo: exampleMarkerTracking.cpp                                *
 * -------------------------------------------------------------------------- *
 * Copyright (c) 2017 Stanford University and the Authors                     *
 *                                                                            *
 * Author(s): Nicholas Bianco                                                 *
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

/// This example solves a basic marker tracking problem using a double
/// pendulum.

#include <OpenSim/Simulation/SimbodyEngine/PinJoint.h>
#include <OpenSim/Simulation/MarkersReference.h>
#include <OpenSim/Actuators/CoordinateActuator.h>
#include <Muscollo/osimMuscollo.h>

using namespace OpenSim;

/// This model is torque-actuated.
Model createDoublePendulumModel() {
    Model model;
    model.setName("double_pendulum");

    using SimTK::Vec3;
    using SimTK::Inertia;

    // Create two links, each with a mass of 1 kg, center of mass at the body's
    // origin, and moments and products of inertia of zero.
    auto* b0 = new OpenSim::Body("b0", 1, Vec3(0), Inertia(1));
    model.addBody(b0);
    auto* b1 = new OpenSim::Body("b1", 1, Vec3(0), Inertia(1));
    model.addBody(b1);

    // Add markers to body origin locations
    // auto* m0 = new Marker("m0", *b0, Vec3(0))
    // auto* m1 = new Marker("m1", *b1, Vec3(0))
    auto* m0 = new Marker();
    auto* m1 = new Marker();
    m0->setName("m0");
    m1->setName("m1");
    m0->setParentFrame(*b0);
    m1->setParentFrame(*b1);
    m0->set_location(Vec3(0));
    m1->set_location(Vec3(0));
    auto* mSet = new MarkerSet();
    model.updateMarkerSet(*mSet);
    model.addMarker(m0);
    model.addMarker(m1);

    // Connect the bodies with pin joints. Assume each body is 1 m long.
    auto* j0 = new PinJoint("j0", model.getGround(), Vec3(0), Vec3(0),
        *b0, Vec3(-1, 0, 0), Vec3(0));
    auto& q0 = j0->updCoordinate();
    q0.setName("q0");
    auto* j1 = new PinJoint("j1",
        *b0, Vec3(0), Vec3(0), *b1, Vec3(-1, 0, 0), Vec3(0));
    auto& q1 = j1->updCoordinate();
    q1.setName("q1");
    model.addJoint(j0);
    model.addJoint(j1);

    auto* tau0 = new CoordinateActuator();
    tau0->setCoordinate(&j0->updCoordinate());
    tau0->setName("tau0");
    tau0->setOptimalForce(1);
    model.addComponent(tau0);

    auto* tau1 = new CoordinateActuator();
    tau1->setCoordinate(&j1->updCoordinate());
    tau1->setName("tau1");
    tau1->setOptimalForce(1);
    model.addComponent(tau1);

    return model;
}

class MucoMarkerTrackingCost : public MucoCost {
OpenSim_DECLARE_CONCRETE_OBJECT(MucoMarkerTrackingCost, MucoCost);
public:
    MucoMarkerTrackingCost() { constructProperties(); }
    
    void setReference(const MarkersReference& ref) {
        m_markref = ref;
        m_marker_names = ref.getNames();
    }
protected:
    void initializeImpl() const override {
        // Cache reference pointers to model markers in order of reference
        // markers.
        for (int i = 0; i < m_marker_names.size(); ++i) {
            const auto& m = getModel().getComponent<Marker>(m_marker_names[i]);
            m_model_markers.emplace_back(&m);
        }
    }
    
    void calcIntegralCostImpl(const SimTK::State& state,
                              double& integrand) const override {
        //const auto& time = state.getTime();
        getModel().realizePosition(state); // why is this needed?
        TimeSeriesTableVec3& markerTable = m_markref.getMarkerTable();
        std::vector<std::string> suffixes(3);
        suffixes[0] = '_x';
        suffixes[1] = '_y';
        suffixes[2] = '_z';


        //SimTK::Array_<SimTK::Vec3> refValues;
        //m_markref.getValues(state, refValues);

        for (int i = 0; i < m_marker_names.size(); ++i) {
            std::string name = m_marker_names[i];

            const auto& modelValue = 
                m_model_markers[i]->getLocationInGround(state);
            
            integrand += (modelValue - refValues[i]).normSqr();
        }
    }
private:
    void constructProperties() {}; // why is this needed?
    MarkersReference m_markref;
    mutable GCVSplineSet m_refsplines;
    mutable std::vector<SimTK::ReferencePtr<const Marker>> m_model_markers;
    SimTK::Array_<std::string> m_marker_names;
};

int main() {

    MucoTool muco;
    muco.setName("double_pendulum_marker_tracking");

    // Define the optimal control problem.
    // ===================================
    MucoProblem& mp = muco.updProblem();

    // Model (dynamics).
    // -----------------
    mp.setModel(createDoublePendulumModel());

    // Bounds.
    // -------
    double finalTime = 1.0;
    mp.setTimeBounds(0, finalTime);
    mp.setStateInfo("j0/q0/value", { -10, 10 });
    mp.setStateInfo("j0/q0/speed", { -50, 50 });
    mp.setStateInfo("j1/q1/value", { -10, 10 });
    mp.setStateInfo("j1/q1/speed", { -50, 50 });
    mp.setControlInfo("tau0", { -100, 100 }); // TODO tighten.
    mp.setControlInfo("tau1", { -100, 100 });

    MucoMarkerTrackingCost markerTracking;
    TimeSeriesTableVec3 refTableVec3;
    refTableVec3.setColumnLabels({"m0", "m1"});
    for (double time = -0.05; time < finalTime+0.05; time += 0.01) {

        SimTK::Real theta0 = (time / 1.0) * 0.5 * SimTK::Pi;
        SimTK::Real theta1 = (time / 1.0) * 0.25 * SimTK::Pi;
        SimTK::Vec3 m0;
        SimTK::Vec3 m1;

        m0[0] = cos(theta0);
        m0[1] = sin(theta1);
        m0[2] = 0;
        m1[0] = m0[0] + cos(theta0 + theta1);
        m1[1] = m1[1] + sin(theta0 + theta1);
        m1[2] = 0;

        SimTK::RowVector_<SimTK::Vec3> markers(2);
        markers.updElt(0, 0) = m0;
        markers.updElt(0, 1) = m1;
        refTableVec3.appendRow(time, markers);
    }

    MarkersReference ref(refTableVec3);
    markerTracking.setReference(ref);
    mp.addCost(markerTracking);

    // Configure the solver.
    // =====================
    MucoTropterSolver& ms = muco.initSolver();
    ms.set_num_mesh_points(50);
    ms.set_verbosity(2);
    ms.set_optim_solver("ipopt");
    ms.set_optim_hessian_approximation("exact");

    // Solve the problem.
    // ==================
    MucoSolution solution = muco.solve();
    solution.write("exampleMarkerTracking_solution.sto");

    muco.visualize(solution);

    return EXIT_SUCCESS;
}