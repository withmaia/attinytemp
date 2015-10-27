wall = 1.5;
l = 62 + 2*wall;
w = 18 + 2*wall;
h = 11;
epsilon = 0.001;

$fn = 16;

module round_cube(l, w, h, r=2) {
    hull() {
        for(x=[0+r, l-r]) {
            for(y=[0+r, w-r]) {
                for(z=[0+r, h-r]) {
                    translate([x, y, z])
                    sphere(r);
                }
            }
        }
    }
}

outr = 2;
inr = 1.3;

difference() {
    // Main shell
    intersection() {
        difference() {
            round_cube(l, w, h*2, r=outr);
            translate([wall, wall, wall])
            round_cube(l-2*wall, w-2*wall, (h-wall)*2, r=inr);
        }
        // Bottom half
        cube([l, w, h]);
    }

    // Plug hole
    hole_w = 6.5; // Maybe 6?
    hole_z = 5.2;
    hole_h = h + 1 - hole_z;
    translate([-1, (w-hole_w)/2, hole_z])
    cube([hole_w, hole_w, hole_h+epsilon]);

    // Air holes
    for (ai=[2:9]) {
        translate([l-2*ai, -epsilon, wall])
        cube([1, w+2*epsilon, h-2*wall]);
    }
}

// Tabs & Notches
// -----------------------------------------------------------------------------

tab_w = 3;
notch_w = tab_w*1.25;
tab_neck_h = 1;
tab_head_h = 1;
notch_h = tab_head_h;
tab_t = 0.7;
tab_in = wall/2;
notch_in = wall - 0.7;

// Tabs
module tab() {
    linear_extrude(height=tab_w) {
        // Neck
        // 3 2
        // 0
        //   1
        polygon([
            [0, 0], [tab_t,-1], [tab_t,tab_neck_h], [0, tab_neck_h]
        ], [
            [0, 1, 2, 3]
        ]);
        // Head
        // 4 3
        //     2
        // 0 1
        translate([0, tab_neck_h, 0])
        polygon([
            [0, 0], [tab_t, 0], [tab_t+tab_in,tab_head_h/2], [tab_t,tab_head_h], [0, tab_head_h]
        ], [
            [0, 1, 2, 3, 4]
        ]);
    }
}

for(x=[l/4, l*3/4]) {
    translate([x+tab_w/2, wall+tab_t, h])
    rotate([90, 00, 270])
    tab();

    translate([x-tab_w/2, w-wall-tab_t, h])
    rotate([90, 00, 90])
    tab();
}

// Top
translate([0, 0, h]) {
    difference() {
        intersection() {

            // Outer shell
            difference() {
                round_cube(l, w, h*2, r=outr);
                translate([wall, wall, wall])
                round_cube(l-2*wall, w-2*wall, (h-wall)*2, r=inr);
            }

            // Top half
            translate([0, 0, h])
            cube([l, w, h]);
        }

        // Notches
        translate([l/4-notch_w/2, wall-notch_in, h+tab_neck_h])
        cube([notch_w, w-2*wall+2*notch_in, notch_h]);
        translate([l*3/4-notch_w/2, wall-notch_in, h+tab_neck_h])
        cube([notch_w, w-2*wall+2*notch_in, notch_h]);

        // Logo impression
        translate([l-w*2/3-w/3/2, w/3/2, h*2-(wall-0.7)])
        linear_extrude(height=1+epsilon) resize([w*2/3, w*2/3]) import("maia-logo.dxf");

    }
}
