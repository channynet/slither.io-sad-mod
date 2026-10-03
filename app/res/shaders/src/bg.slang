import modules.common;

struct vs_input {
  [[vk::location(0)]] float2 pos;
};

struct vs_output {
  float4 clipspace : SV_Position;
  [[vk::location(0)]] float2 uv;
};

[[vk::binding(0, 0)]] ConstantBuffer<Global> global;
[[vk::binding(2, 0)]] Sampler2D bg_tex;

vs_output vs_main(vs_input input) {
  vs_output output;
  output.clipspace = float4(input.pos * 2 - 1, 0.0, 1.0);
  output.uv = input.pos;
  return output;
}

float4 fs_main(vs_output input) {
  float2 sx = input.uv * global.viewport;
  float2 c = global.viewport * 0.5;
  float2 wp = (sx - c) / (global.zoom) + global.view;
  // NTL Grid's interior line pitch: ~102.5 x 86.5 world units.
  // Only spacing is retained; no texture, bevel, rounded corner or tile shading.
  float2 cell = wp / (float2(102.5, 86.5) * max(global.bg_scale, 0.05));
  float2 distanceToLine = abs(frac(cell + 0.5) - 0.5);
  float2 footprint = max(fwidth(cell), float2(0.000001));
  float2 line = 1.0 - smoothstep(float2(0.35), float2(1.15), distanceToLine / footprint);
  float opacity = max(line.x, line.y);
  float shade = lerp(0.19, 0.115, opacity);
  return float4(float3(shade) * global.bg_color, global.bg_opacity);
}
