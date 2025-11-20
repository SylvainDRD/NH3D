#!/bin/bash

shaders_path=$(dirname $0)/src/rendering/shaders

shader_files=$(ls $shaders_path | grep -v ".spv" | grep -v ".inc")

for file in $shader_files
do
    echo "Building $file..."
    glslc --target-env=vulkan1.3 "$shaders_path/$file" -o "$shaders_path/$file.spv"
done

echo "Shaders pre-compilation complete"
