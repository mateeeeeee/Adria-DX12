This is an offline tool which generates the C++ "NvPerfReportDefinitionXXYYY.h" header files.

Example command:
```
python3 profiler_report_generator.py --chip ga10x --outDir=PATH/TO/YOUR/OUTPUT_DIR --pypath pub/ampere
```

* This has been tested with Python 3.5.2
* Please use "profiler_report_generator.py --help" for more details

A pre-generated version of these header files has been deployed to the "NvPerfUtility" include directory.