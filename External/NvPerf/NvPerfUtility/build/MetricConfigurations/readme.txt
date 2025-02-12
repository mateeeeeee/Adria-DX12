This is an offline tool which generates the C++ "NvPerfMetricConfigurationsXXYYY.h" header files.

Example command:
```
python3 metric_configurations_generator.py --chip gb20x --outDir=PATH/TO/YOUR/OUTPUT_DIR --yamlPaths ../../data/MetricConfigurations/gb20x
```

* This has been tested with Python 3.5.2
* Please use "metric_configurations_generator.py --help" for more details

A pre-generated version of these header files has been deployed to the "NvPerfUtility" include directory.