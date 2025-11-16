#!/bin/bash

echo "Screenshot Analysis Pipeline Starting..."

# Create logs directory if it doesn't exist
mkdir -p ../screenshots_logs

# Find all PPM files
ppm_files=(../screenshots/*.ppm)

# Check if any files were found
if [ ! -e "${ppm_files[0]}" ]; then
    echo "No PPM files found in ../screenshots/"
    exit 1
fi

echo "Found ${#ppm_files[@]} PPM files in ../screenshots/"

# Initialize counters and summary
successful=0
failed=0
summary_file="../screenshots_logs/summary.log"

# Start summary log
{
    echo "Screenshot Analysis Pipeline - $(date '+%Y-%m-%d %H:%M:%S')"
    echo "Found ${#ppm_files[@]} PPM files in ../screenshots/"
    echo ""
} > "$summary_file"

# Process each PPM file
for ppm_file in "${ppm_files[@]}"; do
    # Get filename without path and extension
    filename=$(basename "$ppm_file")
    base_name="${filename%.*}"
    log_file="../screenshots_logs/${base_name}.log"

    echo "Processing $filename..."

    # Run analyze_screenshot.py and capture output
    if python3 analyze_screenshot.py --screenshot "$ppm_file" --verbose > "$log_file" 2>&1; then
        echo "  ✓ SUCCESS -> ${base_name}.log"
        echo "Running analyze_screenshot.py on: $filename -> SUCCESS" >> "$summary_file"
        ((successful++))
    else
        echo "  ✗ FAILED -> ${base_name}.log"
        echo "Running analyze_screenshot.py on: $filename -> FAILED" >> "$summary_file"
        ((failed++))
    fi
done

# Finish summary log
{
    echo ""
    echo "Processing complete: $successful/${#ppm_files[@]} files successful"
    if [ $failed -gt 0 ]; then
        echo "Failed files: $failed"
    fi
} >> "$summary_file"

# Print final results
echo ""
echo "Pipeline complete!"
echo "  Successful: $successful"
echo "  Failed: $failed"
echo "  Summary: $summary_file"
echo "  Individual logs in: ../screenshots_logs/"