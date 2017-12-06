// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Radu Serban, Michael Taylor
// =============================================================================
//
// Demonstration of a continuous band track on the track test rig.
//
// =============================================================================

#include "chrono/core/ChFileutils.h"
#include "chrono/utils/ChUtilsInputOutput.h"

#include "chrono_vehicle/ChVehicleModelData.h"
#include "chrono_vehicle/tracked_vehicle/utils/ChIrrGuiDriverTTR.h"
#include "chrono_vehicle/tracked_vehicle/utils/ChTrackTestRig.h"
#include "chrono_vehicle/utils/ChVehicleIrrApp.h"

#include "chrono_models/vehicle/m113/M113_TrackAssemblyBandANCF.h"
#include "chrono_models/vehicle/m113/M113_TrackAssemblyBandBushing.h"

#include "chrono_fea/ChElementShellANCF.h"
#include "chrono_fea/ChLinkDirFrame.h"
#include "chrono_fea/ChLinkPointFrame.h"
#include "chrono_fea/ChMesh.h"
#include "chrono_fea/ChVisualizationFEAmesh.h"

#include "chrono_mkl/ChSolverMKL.h"

using namespace chrono;
using namespace chrono::vehicle;
using namespace chrono::vehicle::m113;

using std::cout;
using std::endl;

// =============================================================================
// USER SETTINGS
// =============================================================================

bool use_JSON = false;
std::string filename("M113/track_assembly/M113_TrackAssemblyBandANCF_Left.json");

double post_limit = 0.2;

// Simulation step size
double step_size = 1e-5;

// Time interval between two render frames
// double render_step_size = 1.0 / 500;
double render_step_size = step_size;

// Output (screenshot captures)
bool img_output = false;

const std::string out_dir = GetChronoOutputPath() + "TRACK_TEST_RIG";

// =============================================================================
int main(int argc, char* argv[]) {
    GetLog() << "Copyright (c) 2017 projectchrono.org\nChrono version: " << CHRONO_VERSION << "\n\n";

    // -------------------------
    // Create the track test rig
    // -------------------------

    ChTrackTestRig* rig = nullptr;
    ChVector<> attach_loc(0, 1, 0);

    if (use_JSON) {
        rig = new ChTrackTestRig(vehicle::GetDataFile(filename), attach_loc);
    } else {
        VehicleSide side = LEFT;
        TrackShoeType type = TrackShoeType::BAND_ANCF;

        std::shared_ptr<ChTrackAssembly> track_assembly;
        switch (type) {
            case TrackShoeType::BAND_BUSHING: {
                auto assembly = std::make_shared<M113_TrackAssemblyBandBushing>(side);
                track_assembly = assembly;
                break;
            }
            case TrackShoeType::BAND_ANCF: {
                auto assembly = std::make_shared<M113_TrackAssemblyBandANCF>(side);
                track_assembly = assembly;
                break;
            }
            default:
                cout << "Track type not supported" << endl;
                return 1;
        }

        rig = new ChTrackTestRig(track_assembly, attach_loc, ChMaterialSurface::SMC);
    }

    // -----------------------------
    // Initialize the track test rig
    // -----------------------------

    ChVector<> rig_loc(0, 0, 2);
    ChQuaternion<> rig_rot(1, 0, 0, 0);
    rig->Initialize(ChCoordsys<>(rig_loc, rig_rot));

    // rig->GetSystem()->Set_G_acc(ChVector<>(0, 0, 0));

    rig->GetTrackAssembly()->SetSprocketVisualizationType(VisualizationType::PRIMITIVES);
    rig->GetTrackAssembly()->SetIdlerVisualizationType(VisualizationType::PRIMITIVES);
    rig->GetTrackAssembly()->SetRoadWheelAssemblyVisualizationType(VisualizationType::PRIMITIVES);
    rig->GetTrackAssembly()->SetRoadWheelVisualizationType(VisualizationType::PRIMITIVES);
    rig->GetTrackAssembly()->SetTrackShoeVisualizationType(VisualizationType::PRIMITIVES);

    ////rig->SetCollide(TrackedCollisionFlag::NONE);
    ////rig->SetCollide(TrackedCollisionFlag::SPROCKET_LEFT | TrackedCollisionFlag::SHOES_LEFT);
    ////rig->GetTrackAssembly()->GetSprocket()->GetGearBody()->SetCollide(false);

    // ---------------------------------------
    // Create the vehicle Irrlicht application
    // ---------------------------------------

    ////ChVector<> target_point = rig->GetPostPosition();
    ////ChVector<> target_point = rig->GetTrackAssembly()->GetIdler()->GetWheelBody()->GetPos();
    ChVector<> target_point = rig->GetTrackAssembly()->GetSprocket()->GetGearBody()->GetPos();

    ChVehicleIrrApp app(rig, NULL, L"Suspension Test Rig");
    app.SetSkyBox();
    app.AddTypicalLights(irr::core::vector3df(30.f, -30.f, 100.f), irr::core::vector3df(30.f, 50.f, 100.f), 250, 130);
    app.SetChaseCamera(ChVector<>(-2.0, 0.0, 0.0), 3.0, 0.0);
    app.SetChaseCameraPosition(target_point + ChVector<>(-2.0, 3, 0));
    app.SetChaseCameraMultipliers(1e-4, 10);
    app.SetTimestep(step_size);
    app.AssetBindAll();
    app.AssetUpdateAll();

    // ------------------------
    // Create the driver system
    // ------------------------

    ChIrrGuiDriverTTR driver(app, post_limit);
    // Set the time response for keyboard inputs.
    double steering_time = 1.0;      // time to go from 0 to max
    double displacement_time = 2.0;  // time to go from 0 to max applied post motion
    driver.SetSteeringDelta(render_step_size / steering_time);
    driver.SetDisplacementDelta(render_step_size / displacement_time * post_limit);
    driver.Initialize();

    // -----------------
    // Initialize output
    // -----------------

    if (ChFileutils::MakeDirectory(out_dir.c_str()) < 0) {
        cout << "Error creating directory " << out_dir << endl;
        return 1;
    }

    // ------------------------------
    // Solver and integrator settings
    // ------------------------------

    auto mkl_solver = std::make_shared<ChSolverMKL<>>();
    rig->GetSystem()->SetSolver(mkl_solver);
    mkl_solver->SetSparsityPatternLock(false);
    rig->GetSystem()->Update();

    rig->GetSystem()->SetTimestepperType(ChTimestepper::Type::HHT);
    auto mystepper = std::dynamic_pointer_cast<ChTimestepperHHT>(rig->GetSystem()->GetTimestepper());
    mystepper->SetAlpha(-0.2);
    mystepper->SetMaxiters(200);
    mystepper->SetAbsTolerances(1e-02);
    mystepper->SetMode(ChTimestepperHHT::ACCELERATION);
    mystepper->SetScaling(true);
    mystepper->SetVerbose(false);
    mystepper->SetStepControl(true);
    mystepper->SetModifiedNewton(false);

    // ---------------
    // Simulation loop
    // ---------------

    // IMPORTANT: Mark completion of system construction
    rig->GetSystem()->SetupInitial();

    // Inter-module communication data
    TerrainForces shoe_forces(1);

    // Number of simulation steps between two 3D view render frames
    int render_steps = (int)std::ceil(render_step_size / step_size);

    // Initialize simulation frame counter
    int step_number = 0;
    int render_frame = 0;

    while (app.GetDevice()->run()) {
        // Debugging output
        const ChFrameMoving<>& c_ref = rig->GetChassisBody()->GetFrame_REF_to_abs();
        const ChVector<>& i_pos_abs = rig->GetTrackAssembly()->GetIdler()->GetWheelBody()->GetPos();
        const ChVector<>& s_pos_abs = rig->GetTrackAssembly()->GetSprocket()->GetGearBody()->GetPos();
        ChVector<> i_pos_rel = c_ref.TransformPointParentToLocal(i_pos_abs);
        ChVector<> s_pos_rel = c_ref.TransformPointParentToLocal(s_pos_abs);
        ////cout << "Time: " << rig->GetSystem()->GetChTime() << endl;
        ////cout << "      idler:    " << i_pos_rel.x << "  " << i_pos_rel.y << "  " << i_pos_rel.z << endl;
        ////cout << "      sprocket: " << s_pos_rel.x << "  " << s_pos_rel.y << "  " << s_pos_rel.z << endl;

        // Render scene
        if (step_number % render_steps == 0) {
            app.BeginScene(true, true, irr::video::SColor(255, 140, 161, 192));
            app.DrawAll();
            app.EndScene();

            if (img_output && step_number > 1000) {
                char filename[100];
                sprintf(filename, "%s/img_%03d.jpg", out_dir.c_str(), render_frame + 1);
                app.WriteImageToFile(filename);
            }

            render_frame++;
        }

        // Collect output data from modules
        double throttle_input = driver.GetThrottle();
        double post_input = driver.GetDisplacement();

        // Update modules (process inputs from other modules)
        double time = rig->GetChTime();
        driver.Synchronize(time);
        rig->Synchronize(time, post_input, throttle_input, shoe_forces);
        app.Synchronize("", 0, throttle_input, 0);

        // Advance simulation for one timestep for all modules
        driver.Advance(step_size);
        rig->Advance(step_size);
        app.Advance(step_size);

        // Increment frame number
        step_number++;

        cout << "Step: " << step_number << "   Time: " << time;
        cout << "  Number of Iterations: " << mystepper->GetNumIterations();
        cout << endl;
    }

    delete rig;

    return 0;
}
