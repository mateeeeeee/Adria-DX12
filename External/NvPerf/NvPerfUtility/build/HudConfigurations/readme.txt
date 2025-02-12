This is an offline tool which generates the C++ "NvPerfHudConfigurationsXXYYY.h" header files.

Example command:
```
python3 hud_configurations_generator.py --chip ga10x --outDir=PATH/TO/YOUR/OUTPUT_DIR --yamlPaths pub/universal pub/ampere pub/ga10x
```

* This has been tested with Python 3.5.2
* Please use "hud_configuration_generator.py --help" for more details

A pre-generated version of these header files has been deployed to the "NvPerfUtility" include directory.