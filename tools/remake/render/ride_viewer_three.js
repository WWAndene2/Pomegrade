// three.js and the two add-ons the ride viewer uses, bundled into one classic script (build_ride_viewer.py runs esbuild on it)
import * as THREE from 'three'; import {ColladaLoader} from 'three/examples/jsm/loaders/ColladaLoader.js'; import {OrbitControls} from 'three/examples/jsm/controls/OrbitControls.js';
window.THREE=THREE; window.ColladaLoader=ColladaLoader; window.OrbitControls=OrbitControls;
