// =============================================================================
// HexSat-100 V2 — Parametric OpenSCAD Model
// =============================================================================
// Hexagonal CubeSat (100 mm flat-to-flat) with 3x Reaction Wheels (N20),
// optical encoders, magnetorquers, and full electronics stack.
//
// Usage:
//   Render full assembly:  openscad hexsat100_v2.scad
//   Export individual STL: openscad -D _part="base" -o base.stl hexsat100_v2.scad
//   Customize via:         openscad hexsat100_v2.scad (opens GUI customizer)
//
// License: CC BY-SA 4.0
// =============================================================================

// include <BOSL2/std.scad>      // Uncomment if BOSL2 installed

// =============================================================================
// [RENDER CONTROL]
// =============================================================================

// Select which part to render ("all" = full assembly)
_part = "all"; // [all, base, top, side_panel, standoff, n20_mount, reaction_wheel, encoder_mount, magnetorquer, sensor_boom, gps_mount, pico2w_mount, hc12_mount, eps_mount]

// Show reference axes (assembly only)
_show_axes = false;

// =============================================================================
// [GLOBAL DIMENSIONS]
// =============================================================================

// Flat-to-flat distance of the hexagon (mm)
// 115 mm ensures 50 mm reaction wheels fit at spec motor positions
// (40,0), (-20,35), (-20,-35) with ~1 mm clearance
flat_to_flat = 115;

// Total height of the satellite structure (mm)
total_height = 120;

// Panel/shell thickness (mm)
wall = 3;

// General 3D printing tolerance (mm)
tolerance = 0.3;

// =============================================================================
// [SCREW SIZES]
// =============================================================================

// M3 hole diameter (mm)
screw_m3 = 3.0 + tolerance;
// M2 hole diameter (mm)
screw_m2 = 2.0 + tolerance;
// M2.5 hole diameter (mm)
screw_m2p5 = 2.5 + tolerance;
// M3 nut trap width across flats (mm)
nut_m3_w = 6.2;
// M3 nut trap height (mm)
nut_m3_h = 2.5;
// M2 nut trap width (mm)
nut_m2_w = 4.2;
// M2 nut trap height (mm)
nut_m2_h = 2.0;

// =============================================================================
// [STRUCTURE]
// =============================================================================

// Standoff / column M3 positions [x, y]
standoff_pos = [
    [-20, -20], [20, -20],
    [-20,  20], [20,  20]
];

// Number of side panels
side_count = 6;

// Side panel width (mm)
// Default: flat_to_flat / sqrt(3) ≈ 57.74 (full hex side length)
// Spec: 50 mm (narrower, leaves edge clearance)
panel_width = flat_to_flat / sqrt(3);

// =============================================================================
// [ADCS — REACTION WHEELS]
// =============================================================================

// Reaction wheel motor positions [x, y]
rw_x = [ 40,   0];
rw_y = [-20,  35];
rw_z = [-20, -35];

// N20 motor mount cavity (internal, with tolerance)
motor_cav_w = 12.0 + tolerance;  // 12.4 mm target
motor_cav_h = 10.0 + tolerance;  // 10.4 mm target
motor_cav_d = 15.0 + tolerance;  // 15.5 mm target

// Motor mount wall thickness (mm)
motor_mount_wall = 3;

// Reaction wheel
wheel_dia    = 50;   // Outer diameter (mm)
wheel_thick  = 6;    // Thickness (mm)
wheel_hub    = 8;    // Central hub diameter (mm)
wheel_shaft  = 3.1;  // Shaft hole diameter (mm)
wheel_grv_w  = 4;    // Peripheral groove width (mm)
wheel_grv_d  = 3;    // Peripheral groove depth (mm)
wheel_grub   = screw_m3;  // Grub screw hole

// Encoder
encoder_base_w = 20;
encoder_base_d = 15;
encoder_base_h = 3;

// =============================================================================
// [ADCS — MAGNETORQUERS]
// =============================================================================

// Ferrite core
mtq_core_dia = 8;    // Core diameter (mm)
mtq_core_len = 70;   // Core length (mm)
mtq_clip_w   = 3;    // Clip wall thickness (mm)

// MTQ orientation axes: X, Y, Z (see assembly)

// =============================================================================
// [SENSORS]
// =============================================================================

// Magnetometer boom
boom_len       = 70;   // Length from structure (mm)
boom_w         = 10;   // Width (mm)
boom_thick     = 3;    // Thickness (mm)
boom_plat      = 20;   // Platform size (mm) 20×20
boom_extra_pos = 100;  // Second position for comparison (mm)

// GPS (NEO-7M)
gps_mount_w = 30;
gps_mount_d = 30;
gps_mount_h = 3;

// =============================================================================
// [ELECTRONICS — MOUNTING PLATFORMS]
// =============================================================================

// Pico 2W
pico_mount_w = 24;  // Slightly larger than PCB 21mm
pico_mount_d = 55;  // Slightly larger than PCB 51mm
pico_mount_h = 3;

// HC-12
hc12_mount_w = 24;  // Slightly larger than module 19mm
hc12_mount_d = 32;  // Slightly larger than module 27mm
hc12_mount_h = 3;

// EPS (placeholder — adjust to your EPS dimensions)
eps_mount_w = 44;
eps_mount_d = 44;
eps_mount_h = 3;

// =============================================================================
// [ASSEMBLY Z-HEIGHTS]
// =============================================================================

// Z-stack (relative to base bottom at z=0)
z_base_top   = wall;              // Base plate top
z_eps        = 5;                 // EPS shelf
z_obc        = 40;                // OBC shelf (~mid-height)
z_comms      = 75;                // COMMS shelf
z_gps        = 95;                // GPS shelf
z_top_bottom = total_height - wall;  // Top plate bottom

// Reaction wheels sit on base plate at z = z_base_top
// Magnetometer boom at z_mag_boom
z_mag_boom = 50;

// =============================================================================
// [APPEARANCE]
// =============================================================================

// Part color (for assembly preview)
part_color = "Black"; // [Black, DarkGray, Silver, White, Custom]
part_color_custom = "#1a1a1a";

// =============================================================================
//  UTILITY FUNCTIONS & MODULES
// =============================================================================

function _hex_r(ftf) = ftf / sqrt(3);
function _side_w(ftf) = _hex_r(ftf);
function _side_angle(i) = i * 60 + 30;  // Center of each flat side

// -- Hexagon shape (2D) --
module _hexagon(ftf, center = true) {
    r = _hex_r(ftf);
    circle(r = r, $fn = 6);
}

// -- Regular polygon for screw holes (to avoid $fn on cylinders) --
module _screw_hole(d, h) {
    cylinder(d = d, h = h + 0.1, $fn = 20);
}

module _m3_hole(h = 10) {
    _screw_hole(screw_m3, h);
}

module _m2_hole(h = 10) {
    _screw_hole(screw_m2, h);
}

module _m2p5_hole(h = 10) {
    _screw_hole(screw_m2p5, h);
}

// -- Nut trap --
module _m3_nut_trap(h = nut_m3_h) {
    cylinder(d = nut_m3_w, h = h, $fn = 6);
}

// -- Countersink for M3 flat head --
module _m3_csk(h = wall) {
    cylinder(d1 = screw_m3, d2 = 6, h = h, $fn = 20);
}

// -- Rounded rectangle (2D) --
module _rounded_rect(w, d, r = 2) {
    offset(r = r) offset(delta = -r)
        square([w, d], center = true);
}

// -- Filleted cylinder --
module _filleted_cyl(d, h, rf = 1) {
    // Simple chamfered cylinder
    cylinder(d = d, h = h, $fn = 48);
}

// =============================================================================
//  1. BASE HEXAGONAL PLATE
// =============================================================================

module base_hex() {
    color(part_color) difference() {
        // Main plate
        linear_extrude(height = wall, convexity = 3)
            _hexagon(flat_to_flat, center = true);

        // Standoff M3 holes (4×)
        for (p = standoff_pos) {
            translate([p.x, p.y, -0.05])
                _m3_hole(wall + 0.1);
        }

        // Reaction wheel motor mount holes (M3)
        for (pos = [rw_x, rw_y, rw_z]) {
            translate([pos.x, pos.y, -0.05])
                _m3_hole(wall + 0.1);
        }

        // Lightening / cable pass-through cutouts
        cable_r = 4;
        for (a = [0 : 60 : 300]) {
            dx = _hex_r(flat_to_flat) * 0.6 * cos(a + 30);
            dy = _hex_r(flat_to_flat) * 0.6 * sin(a + 30);
            translate([dx, dy, -0.05])
                cylinder(d = cable_r * 2, h = wall + 0.1, $fn = 24);
        }
    }
}

// =============================================================================
//  2. TOP HEXAGONAL PLATE
// =============================================================================

module top_hex() {
    color(part_color) difference() {
        linear_extrude(height = wall, convexity = 3)
            _hexagon(flat_to_flat, center = true);

        // Standoff M3 holes (aligned with base)
        for (p = standoff_pos) {
            translate([p.x, p.y, -0.05])
                _m3_hole(wall + 0.1);
        }

        // Antenna pass-through (GPS / COMMS)
        translate([0, 25, -0.05])
            cylinder(d = 6, h = wall + 0.1, $fn = 24);

        // Ventilation / access slots
        for (a = [0 : 60 : 300]) {
            dx = _hex_r(flat_to_flat) * 0.5 * cos(a + 30);
            dy = _hex_r(flat_to_flat) * 0.5 * sin(a + 30);
            translate([dx, dy, -0.05])
                cylinder(d = 5, h = wall + 0.1, $fn = 20);
        }

        // GPS cable pass-through
        translate([0, -15, -0.05])
            cylinder(d = 5, h = wall + 0.1, $fn = 20);
    }
}

// =============================================================================
//  3. SIDE PANEL
// =============================================================================

module side_panel() {
    side_w = panel_width;
    side_h = total_height;
    vent_w = 4;
    vent_gap = 6;

    color(part_color) difference() {
        // Main panel
        cube([side_w, wall, side_h], center = true);

        // Ventilation slots
        for (z = [-side_h/2 + 10 : vent_gap + vent_w : side_h/2 - 10]) {
            translate([0, 0, z])
                cube([side_w - 10, wall + 0.2, vent_w], center = true);
        }

        // Connector cutouts (top and bottom zones)
        // Power input
        translate([-side_w/4, 0, -side_h/3])
            cube([8, wall + 0.2, 6], center = true);

        // Antenna cutout (top)
        translate([side_w/4, 0, side_h/3])
            cube([6, wall + 0.2, 10], center = true);

        // UART / programming access
        translate([0, 0, -side_h/6])
            cube([5, wall + 0.2, 4], center = true);
    }
}

// =============================================================================
//  4. M3 STANDOFF / STRUCTURAL COLUMN
// =============================================================================

module standoff(height = total_height) {
    screw_len = 6;  // Threaded insert depth at each end

    color("Silver") difference() {
        // Main column
        cylinder(d = screw_m3 + 2 * wall, h = height, $fn = 32);

        // Through-hole for M3
        translate([0, 0, -0.05])
            _screw_hole(screw_m3, height + 0.1);

        // Counterbore for screw heads (top and bottom)
        translate([0, 0, -0.05])
            cylinder(d = 5.5, h = screw_len, $fn = 20);
        translate([0, 0, height - screw_len - 0.05])
            cylinder(d = 5.5, h = screw_len + 0.1, $fn = 20);
    }
}

// =============================================================================
//  5. N20 MOTOR MOUNT
// =============================================================================

module n20_motor_mount() {
    outer_w = motor_cav_w + 2 * motor_mount_wall;
    outer_h = motor_cav_h + 2 * motor_mount_wall;
    mount_plate_w = outer_w + 10;
    mount_plate_d = motor_mount_wall + 4;

    color(part_color) union() {
        // Base mounting plate (attaches to structure with M3)
        difference() {
            translate([-mount_plate_w / 2, -mount_plate_d / 2, 0])
                cube([mount_plate_w, mount_plate_d, motor_mount_wall]);

            // M3 mounting holes (2×)
            for (x = [-mount_plate_w / 4, mount_plate_w / 4]) {
                translate([x, 0, -0.05])
                    _m3_hole(motor_mount_wall + 0.1);
            }
        }

        // Motor cavity housing
        translate([-motor_cav_w / 2, motor_mount_wall, wall])
            difference() {
                // Outer shell
                cube([outer_w, motor_cav_d + motor_mount_wall, outer_h]);

                // Inner cavity
                translate([motor_mount_wall, motor_mount_wall, -0.05])
                    cube([motor_cav_w, motor_cav_d + 0.1, motor_cav_h + 0.1]);

                // Shaft exit hole (top face)
                translate([outer_w / 2, motor_cav_d + motor_mount_wall, -0.05])
                    cylinder(d = 4, h = outer_h + 0.1, $fn = 20);

                // M2 screw holes for motor body
                for (z = [motor_mount_wall + 3, motor_mount_wall + 12]) {
                    translate([outer_w / 2 - 3, -0.05, z])
                        rotate([-90, 0, 0])
                            _m2_hole(motor_mount_wall + motor_cav_d + 0.1);
                }

                // Access cutout for grub screw / shaft
                translate([outer_w / 2, motor_cav_d / 2, wall + 2])
                    cube([6, 8, 6], center = true);
            }
    }
}

// =============================================================================
//  6. REACTION WHEEL (inertia disc)
// =============================================================================

module reaction_wheel() {
    grv_od = wheel_dia - 2 * wheel_grv_d;
    grv_id = grv_od - wheel_grv_w;
    hub_h = wheel_thick + 2;  // Hub extends above/below disc

    color(part_color) difference() {
        union() {
            // Main disc
            linear_extrude(height = wheel_thick, convexity = 3)
                difference() {
                    circle(d = wheel_dia, $fn = 64);

                    // Central hub bore
                    circle(d = wheel_shaft, $fn = 20);
                }

            // Central hub (raised boss)
            translate([0, 0, -1])
                cylinder(d = wheel_hub, h = wheel_thick + 2, $fn = 32);
        }

        // Shaft hole through hub
        translate([0, 0, -1.05])
            _screw_hole(wheel_shaft, wheel_thick + 2.1);

        // Peripheral groove for metal ring
        translate([0, 0, -0.05])
            difference() {
                cylinder(d = grv_od, h = wheel_thick + 0.1, $fn = 64);
                translate([0, 0, -0.05])
                    cylinder(d = grv_id, h = wheel_thick + 0.2, $fn = 64);
            }

        // Grub screw hole (M3 radial)
        translate([wheel_hub / 2 + 2, 0, wheel_thick / 2])
            rotate([0, 90, 0])
                _screw_hole(screw_m3, 6);

        // Lightening holes (non-structural)
        for (a = [0 : 45 : 315]) {
            translate([wheel_dia * 0.25 * cos(a), wheel_dia * 0.25 * sin(a), -0.05])
                cylinder(d = 6, h = wheel_thick + 0.1, $fn = 16);
        }
    }
}

// =============================================================================
//  7. OPTICAL ENCODER MOUNT
// =============================================================================

module optical_encoder_mount() {
    arm_w = 6;
    arm_len = 15;
    sensor_x = encoder_base_w / 2 - 3;

    color(part_color) union() {
        // Base plate
        difference() {
            cube([encoder_base_w, encoder_base_d, encoder_base_h], center = true);
            // Mounting holes
            for (x = [-encoder_base_w / 3, encoder_base_w / 3]) {
                translate([x, encoder_base_d / 4, 0])
                    _m2_hole(encoder_base_h + 0.1);
            }
            // Center pass-through for wheel
            translate([0, -encoder_base_d / 4, -0.05])
                cylinder(d = 12, h = encoder_base_h + 0.1, $fn = 24);
        }

        // Sensor support arm
        translate([-encoder_base_w / 2 + arm_w / 2,
                   -encoder_base_d / 2 - arm_len / 2,
                   encoder_base_h])
            cube([arm_w, arm_len, 2], center = true);

        // Alignment slot indicator
        %translate([0, encoder_base_d / 2 - 2, encoder_base_h])
            cube([10, 1, 0.5], center = true);
    }
}

// =============================================================================
//  8. MAGNETORQUER CLIP
// =============================================================================

module magnetorquer_clip() {
    gap = 1;  // Insertion gap for tolerance
    clip_d = mtq_core_dia + tolerance + 2 * mtq_clip_w;
    clip_len = 12;  // Each clip segment length
    strap_w = 5;

    color(part_color) union() {
        // Core cradle (lower half)
        difference() {
            cube([clip_len, clip_d, mtq_clip_w], center = true);
            translate([0, 0, mtq_clip_w / 2 - gap / 2])
                cube([clip_len + 0.1, mtq_core_dia + tolerance, mtq_clip_w], center = true);
        }

        // M3 mounting tab
        translate([-clip_len / 2 - 4, -clip_d / 2, 0])
            difference() {
                cube([8, 8, mtq_clip_w], center = true);
                translate([0, 0, -0.05])
                    _m3_hole(mtq_clip_w + 0.1);
            }

        // Second M3 tab
        translate([clip_len / 2 + 4, clip_d / 2, 0])
            difference() {
                cube([8, 8, mtq_clip_w], center = true);
                translate([0, 0, -0.05])
                    _m3_hole(mtq_clip_w + 0.1);
            }

        // Retention strap (printed flexible or separate part)
        translate([-clip_len / 2, -clip_d / 2 - strap_w, 0])
            cube([clip_len, strap_w, mtq_clip_w], center = true);
    }
}

// =============================================================================
//  9. SENSOR BOOM (magnetometer arm)
// =============================================================================

module sensor_boom() {
    mount_hole_spacing = 16;

    color(part_color) union() {
        // Main arm
        difference() {
            cube([boom_w, boom_len, boom_thick], center = true);

            // Weight-reduction slots
            for (y = [-boom_len / 4, 0, boom_len / 4]) {
                translate([0, y, -0.05])
                    cube([boom_w - 4, 3, boom_thick + 0.1], center = true);
            }

            // Adjustment slots for 70 mm and 100 mm positions
            for (y = [boom_len / 2 - 30, boom_len / 2]) {
                translate([0, -boom_len / 2 + y, -0.05])
                    cube([3, 6, boom_thick + 0.1], center = true);
            }
        }

        // Platform at end (for HMC5883L)
        translate([0, boom_len / 2 - boom_plat / 2, boom_thick / 2])
            difference() {
                cube([boom_plat, boom_plat, boom_thick], center = true);

                // M2.5 mounting holes for sensor
                for (x = [-4, 4], y = [-4, 4]) {
                    translate([x, y, -0.05])
                        _m2_hole(boom_thick + 0.1);
                }
            }

        // Mounting base (attaches to structure)
        translate([0, -boom_len / 2 + 6, boom_thick / 2])
            difference() {
                cube([14, 12, boom_thick], center = true);
                for (x = [-mount_hole_spacing / 2, mount_hole_spacing / 2]) {
                    translate([x, 0, -0.05])
                        _m3_hole(boom_thick + 0.1);
                }
            }
    }
}

// =============================================================================
//  10. GPS MOUNT (NEO-7M)
// =============================================================================

module gps_mount() {
    // NEO-7M module: ~25 mm × 25 mm,  4 holes at corners
    hole_spacing = 20;

    color(part_color) difference() {
        cube([gps_mount_w, gps_mount_d, gps_mount_h], center = true);

        // M2.5 mounting holes (4×)
        for (x = [-hole_spacing / 2, hole_spacing / 2],
             y = [-hole_spacing / 2, hole_spacing / 2]) {
            translate([x, y, -0.05])
                _m2p5_hole(gps_mount_h + 0.1);
        }

        // Central access hole for cable
        translate([0, 0, -0.05])
            cylinder(d = 8, h = gps_mount_h + 0.1, $fn = 24);

        // Attachment holes to structure (M3)
        for (x = [-gps_mount_w / 3, gps_mount_w / 3]) {
            translate([x, 0, -0.05])
                _m3_hole(gps_mount_h + 0.1);
        }
    }
}

// =============================================================================
//  11. PICO 2W MOUNT
// =============================================================================

module pico2w_mount() {
    // Pico 2W: 21 mm × 51 mm, castellated edges
    // Mounting holes at corners: roughly (1.5, 1.5) from edges
    pcb_w = 21;
    pcb_d = 51;
    hole_x = pcb_w - 3;  // 18 mm spacing
    hole_y = pcb_d - 3;  // 48 mm spacing

    color(part_color) difference() {
        // Platform
        cube([pico_mount_w, pico_mount_d, pico_mount_h], center = true);

        // M2 mounting holes (matching Pico PCB)
        for (x = [-hole_x / 2, hole_x / 2],
             y = [-hole_y / 2, hole_y / 2]) {
            translate([x, y, -0.05])
                _m2_hole(pico_mount_h + 0.1);
        }

        // Standoff attachment holes to structure (M3)
        for (x = [-pico_mount_w / 3, pico_mount_w / 3]) {
            translate([x, 0, -0.05])
                _m3_hole(pico_mount_h + 0.1);
        }

        // USB access cutout
        translate([0, pico_mount_d / 2, -0.05])
            cube([10, 4, pico_mount_h + 0.1], center = true);

        // GPIO access / cable routing
        translate([0, 0, -0.05])
            cube([pico_mount_w - 6, pico_mount_d - 6, pico_mount_h + 0.1], center = true);
    }
}

// =============================================================================
//  12. HC-12 MOUNT (COMMS)
// =============================================================================

module hc12_mount() {
    // HC-12 module: ~19 × 27 mm
    // 7 pins on 2.54 mm pitch, 2 rows × 4 (but really one row of 7)
    mod_w = 19;
    mod_d = 27;

    color(part_color) difference() {
        cube([hc12_mount_w, hc12_mount_d, hc12_mount_h], center = true);

        // Module cavity (recess)
        translate([0, 0, hc12_mount_h / 4])
            cube([mod_w + tolerance, mod_d + tolerance, hc12_mount_h / 2 + 0.1], center = true);

        // Pin header access slot
        translate([-mod_w / 2 + 2.54, 0, -0.05])
            cube([2.54 * 6, 3, hc12_mount_h + 0.1], center = true);

        // Antenna clearance
        translate([hc12_mount_w / 2, -hc12_mount_d / 4, -0.05])
            cylinder(d = 4, h = hc12_mount_h + 0.1, $fn = 16);

        // M3 attachment to structure
        for (x = [-hc12_mount_w / 3, hc12_mount_w / 3]) {
            translate([x, 0, -0.05])
                _m3_hole(hc12_mount_h + 0.1);
        }
    }
}

// =============================================================================
//  13. EPS MOUNT
// =============================================================================

module eps_mount() {
    color(part_color) difference() {
        cube([eps_mount_w, eps_mount_d, eps_mount_h], center = true);

        // Central cutout for EPS components (placeholder)
        translate([0, 0, -0.05])
            cube([eps_mount_w - 8, eps_mount_d - 8, eps_mount_h + 0.1], center = true);

        // M3 attachment to standoffs
        for (x = [-eps_mount_w / 3, eps_mount_w / 3],
             y = [-eps_mount_d / 3, eps_mount_d / 3]) {
            translate([x, y, -0.05])
                _m3_hole(eps_mount_h + 0.1);
        }

        // Cable pass-throughs
        for (x = [-eps_mount_w / 4, eps_mount_w / 4]) {
            translate([x, 0, -0.05])
                cylinder(d = 6, h = eps_mount_h + 0.1, $fn = 16);
        }
    }
}

// =============================================================================
//  14. MTQ CORE (visual reference only — not printed)
// =============================================================================

module _mtq_core() {
    color("DarkRed")
        cylinder(d = mtq_core_dia, h = mtq_core_len, $fn = 24, center = true);
}

// =============================================================================
//  15. N20 MOTOR (visual reference only — not printed)
// =============================================================================

module _n20_motor() {
    motor_body_d = 10;
    motor_body_l = 10;
    gearbox_w = 12;
    gearbox_h = 10;
    gearbox_l = 15;
    shaft_d = 3;
    shaft_l = 10;

    color("Silver") union() {
        // Motor body
        translate([0, 0, -motor_body_l / 2])
            cylinder(d = motor_body_d, h = motor_body_l, $fn = 24);

        // Gearbox
        translate([-gearbox_w / 2, -gearbox_h / 2, -motor_body_l])
            cube([gearbox_w, gearbox_h, gearbox_l]);

        // Shaft
        translate([0, 0, -motor_body_l - gearbox_l])
            cylinder(d = shaft_d, h = shaft_l, $fn = 16);
    }
}

// =============================================================================
//  FULL ASSEMBLY
// =============================================================================

module assembly() {
    // Reference axes
    if (_show_axes) {
        %translate([0, 0, -5]) {
            cylinder(d = 2, h = 130, color("Red"));
            translate([0, -1, 0]) cube([70, 2, 2]);
            translate([-1, 0, 0]) cube([2, 70, 2]);
        }
    }

    // -- Structure --
    // Base
    translate([0, 0, 0])
        base_hex();

    // Standoffs (4×) — span from base top to top plate bottom
    standoff_len = total_height - 2 * wall;
    for (p = standoff_pos) {
        translate([p.x, p.y, wall])
            standoff(standoff_len);
    }

    // Side panels (6×)
    // Positioned at apothem distance from center, flush with the outer edge
    apothem = flat_to_flat / 2;
    panel_radius = apothem + wall / 2;  // Panel center protrudes half-thickness
    for (i = [0 : side_count - 1]) {
        a = _side_angle(i);
        translate([
            panel_radius * cos(a),
            panel_radius * sin(a),
            total_height / 2
        ])
            rotate([0, 0, a - 90])
                side_panel();
    }

    // Top
    translate([0, 0, total_height - wall])
        top_hex();

    // -- Electronics stack (mounted on standoffs) --
    // EPS shelf
    translate([0, 0, z_eps])
        eps_mount();

    // OBC shelf
    translate([0, 0, z_obc])
        pico2w_mount();

    // COMMS shelf
    translate([0, 0, z_comms])
        hc12_mount();

    // GPS on top
    translate([0, 0, z_gps])
        gps_mount();

    // -- ADCS: Reaction Wheels --
    // RW-X
    translate([rw_x.x, rw_x.y, z_base_top]) {
        rotate([180, 0, 0])
            n20_motor_mount();
        translate([0, 0, 8])
            reaction_wheel();
        translate([0, 0, 18])
            optical_encoder_mount();
        %_n20_motor();
    }

    // RW-Y
    translate([rw_y.x, rw_y.y, z_base_top]) {
        rotate([180, 0, 120])
            n20_motor_mount();
        translate([0, 0, 8])
            reaction_wheel();
        translate([0, 0, 18])
            optical_encoder_mount();
        %_n20_motor();
    }

    // RW-Z
    translate([rw_z.x, rw_z.y, z_base_top]) {
        rotate([180, 0, 240])
            n20_motor_mount();
        translate([0, 0, 8])
            reaction_wheel();
        translate([0, 0, 18])
            optical_encoder_mount();
        %_n20_motor();
    }

    // -- ADCS: Magnetorquers --
    // MTQ-X (along X axis)
    translate([15, 0, 25])
        rotate([0, 90, 0]) {
            magnetorquer_clip();
            %translate([0, 0, -mtq_core_len / 2])
                _mtq_core();
        }

    // MTQ-Y (along Y axis)
    translate([0, 15, 35])
        rotate([90, 0, 0]) {
            magnetorquer_clip();
            %translate([0, 0, -mtq_core_len / 2])
                _mtq_core();
        }

    // MTQ-Z (along Z axis)
    translate([0, 0, 60])
        rotate([0, 0, 0]) {
            magnetorquer_clip();
            %translate([0, 0, -mtq_core_len / 2])
                _mtq_core();
        }

    // -- Sensor Boom (magnetometer) --
    // Position at (0, +70, z_mag_boom), pointing radially outward
    translate([0, _hex_r(flat_to_flat) + wall, z_mag_boom])
        rotate([0, 0, 90])
            sensor_boom();
}

// =============================================================================
//  RENDER SELECTOR
// =============================================================================

if (_part == "all") {
    assembly();

} else if (_part == "base") {
    base_hex();

} else if (_part == "top") {
    top_hex();

} else if (_part == "side_panel") {
    side_panel();

} else if (_part == "standoff") {
    standoff(total_height);

} else if (_part == "n20_mount") {
    n20_motor_mount();

} else if (_part == "reaction_wheel") {
    reaction_wheel();

} else if (_part == "encoder_mount") {
    optical_encoder_mount();

} else if (_part == "magnetorquer") {
    magnetorquer_clip();

} else if (_part == "sensor_boom") {
    sensor_boom();

} else if (_part == "gps_mount") {
    gps_mount();

} else if (_part == "pico2w_mount") {
    pico2w_mount();

} else if (_part == "hc12_mount") {
    hc12_mount();

} else if (_part == "eps_mount") {
    eps_mount();

} else {
    echo("ERROR: Unknown _part value. Use: all, base, top, side_panel, standoff, n20_mount, reaction_wheel, encoder_mount, magnetorquer, sensor_boom, gps_mount, pico2w_mount, hc12_mount, eps_mount");
    assembly();
}
