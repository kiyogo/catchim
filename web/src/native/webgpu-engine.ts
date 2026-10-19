/**
 * High-Performance WebGPU Rendering & Compute Engine
 * Implements GPU-accelerated video compositing, compute shaders, and texture pooling.
 */

export interface GpuDeviceState {
	adapter: GPUAdapter | null;
	device: GPUDevice | null;
	isAvailable: boolean;
	preferredFormat: GPUTextureFormat;
}

export interface TexturePoolEntry {
	texture: GPUTexture;
	width: number;
	height: number;
	format: GPUTextureFormat;
	lastUsed: number;
}

class WebGpuEngine {
	private state: GpuDeviceState = {
		adapter: null,
		device: null,
		isAvailable: false,
		preferredFormat: "bgra8unorm",
	};
	private initPromise: Promise<boolean> | null = null;
	private texturePool: TexturePoolEntry[] = [];
	private maxPoolSize = 32;

	// Shaders and Pipelines
	private compositorPipeline: GPURenderPipeline | null = null;
	private layerQuadPipeline: GPURenderPipeline | null = null;
	private blurPipelineH: GPUComputePipeline | null = null;
	private blurPipelineV: GPUComputePipeline | null = null;
	private colorGradePipeline: GPUComputePipeline | null = null;
	private chromaKeyPipeline: GPUComputePipeline | null = null;
	private waveformPipeline: GPUComputePipeline | null = null;
	private sampler: GPUSampler | null = null;
	private whiteTexture: GPUTexture | null = null;
	private effectBuffers: GPUBuffer[] = [];
	private effectBufferIndex = 0;

	async initialize(): Promise<boolean> {
		if (this.initPromise) {
			return this.initPromise;
		}

		this.initPromise = (async () => {
			if (typeof navigator === "undefined" || !navigator.gpu) {
				console.info("[WebGPU] navigator.gpu unavailable in this environment, using accelerated WASM fallback.");
				this.state.isAvailable = false;
				return false;
			}

			try {
				const adapter = await navigator.gpu.requestAdapter({
					powerPreference: "high-performance",
				});
				if (!adapter) {
					console.warn("[WebGPU] Failed to acquire high-performance GPUAdapter.");
					this.state.isAvailable = false;
					return false;
				}

				const device = await adapter.requestDevice({
					requiredLimits: {
						maxTextureDimension2D: Math.min(8192, adapter.limits.maxTextureDimension2D),
					},
				});

				const preferredFormat = navigator.gpu.getPreferredCanvasFormat();
				this.state = {
					adapter,
					device,
					isAvailable: true,
					preferredFormat,
				};

				device.lost.then((info: any) => {
					console.warn("[WebGPU] GPUDevice lost:", info?.message);
					this.state.isAvailable = false;
					this.state.device = null;
					this.compositorPipeline = null;
					this.layerQuadPipeline = null;
					this.blurPipelineH = null;
					this.blurPipelineV = null;
					this.colorGradePipeline = null;
					this.chromaKeyPipeline = null;
					this.texturePool = [];
					this.initPromise = null;
					if (typeof window !== "undefined") (window as any).__OPENCUT_WEBGPU_ACTIVE__ = false;
				});

				this.sampler = device.createSampler({
					magFilter: "linear",
					minFilter: "linear",
					mipmapFilter: "linear",
					addressModeU: "clamp-to-edge",
					addressModeV: "clamp-to-edge",
				});
				this.whiteTexture = device.createTexture({ size: [1, 1], format: "rgba8unorm", usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST });
				device.queue.writeTexture({ texture: this.whiteTexture }, new Uint8Array([255, 255, 255, 255]), {}, [1, 1]);

				await this.initPipelines();
				console.info("%c[WebGPU] Engine initialized successfully with hardware acceleration.", "color: #10b981; font-weight: bold; font-size: 13px;");
				if (typeof window !== "undefined") {
					(window as any).__OPENCUT_WEBGPU_ACTIVE__ = true;
					(window as any).__OPENCUT_GPU_DEVICE__ = device;
					(window as any).__OPENCUT_ENGINE__ = {
						backend: "WebGPU",
						timeline: "Canvas 2D",
						device,
						adapter,
						isWebGpuActive: true,
						version: "2.1.0",
					};
				}
				return true;
			} catch (err) {
				console.warn("[WebGPU] Initialization error:", err);
				this.state.isAvailable = false;
				return false;
			}
		})();

		return this.initPromise;
	}

	isWebGpuSupported(): boolean {
		return typeof navigator !== "undefined" && !!navigator.gpu;
	}

	get isAvailable(): boolean {
		return this.state.isAvailable && this.state.device !== null;
	}

	get device(): GPUDevice | null {
		return this.state.device;
	}

	get preferredFormat(): GPUTextureFormat {
		return this.state.preferredFormat;
	}

	private async initPipelines(): Promise<void> {
		const device = this.state.device;
		if (!device) return;

		// 1. Compositor WGSL Shader
		const compositorWgsl = `
			struct VertexOutput {
				@builtin(position) position: vec4<f32>,
				@location(0) uv: vec2<f32>,
			};

			@vertex
			fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
				var pos = array<vec2<f32>, 4>(
					vec2<f32>(-1.0, -1.0),
					vec2<f32>( 1.0, -1.0),
					vec2<f32>(-1.0,  1.0),
					vec2<f32>( 1.0,  1.0)
				);
				var uvs = array<vec2<f32>, 4>(
					vec2<f32>(0.0, 1.0),
					vec2<f32>(1.0, 1.0),
					vec2<f32>(0.0, 0.0),
					vec2<f32>(1.0, 0.0)
				);
				var out: VertexOutput;
				out.position = vec4<f32>(pos[vertexIndex], 0.0, 1.0);
				out.uv = uvs[vertexIndex];
				return out;
			}

			@group(0) @binding(0) var tex: texture_2d<f32>;
			@group(0) @binding(1) var smp: sampler;

			@fragment
			fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
				return textureSample(tex, smp, in.uv);
			}
		`;

		const compositorModule = device.createShaderModule({ code: compositorWgsl });
		this.compositorPipeline = device.createRenderPipeline({
			layout: "auto",
			vertex: {
				module: compositorModule,
				entryPoint: "vs_main",
			},
			fragment: {
				module: compositorModule,
				entryPoint: "fs_main",
				targets: [
					{
						format: this.state.preferredFormat,
						blend: {
							color: {
								srcFactor: "src-alpha",
								dstFactor: "one-minus-src-alpha",
								operation: "add",
							},
							alpha: {
								srcFactor: "one",
								dstFactor: "one-minus-src-alpha",
								operation: "add",
							},
						},
					},
				],
			},
			primitive: {
				topology: "triangle-strip",
			},
		});
 
		// 1b. Layer Quad Transform Pipeline with Alpha Blending
		const layerQuadWgsl = `
			struct QuadUniforms {
				pos0: vec2<f32>,
				pos1: vec2<f32>,
				pos2: vec2<f32>,
				pos3: vec2<f32>,
				opacity: f32,
				_pad0: f32,
				_pad1: f32,
				_pad2: f32,
				canvasSize: vec2<f32>,
				maskMode: f32,
				_pad3: f32,
			};

			@group(0) @binding(0) var tex: texture_2d<f32>;
			@group(0) @binding(1) var smp: sampler;
			@group(0) @binding(2) var<uniform> quad: QuadUniforms;
			@group(0) @binding(3) var mask_tex: texture_2d<f32>;

			struct VertexOutput {
				@builtin(position) position: vec4<f32>,
				@location(0) uv: vec2<f32>,
			};

			@vertex
			fn vs_main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {
				var positions = array<vec2<f32>, 4>(
					quad.pos0,
					quad.pos1,
					quad.pos2,
					quad.pos3
				);
				var uvs = array<vec2<f32>, 4>(
					vec2<f32>(0.0, 0.0),
					vec2<f32>(1.0, 0.0),
					vec2<f32>(0.0, 1.0),
					vec2<f32>(1.0, 1.0)
				);
				var out: VertexOutput;
				out.position = vec4<f32>(positions[vertexIndex], 0.0, 1.0);
				out.uv = uvs[vertexIndex];
				return out;
			}

			@fragment
			fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
				var c = textureSample(tex, smp, in.uv);
				var maskAlpha = textureSample(mask_tex, smp, in.position.xy / quad.canvasSize).a;
				if (quad.maskMode > 1.5) { maskAlpha = 1.0 - maskAlpha; }
				return vec4<f32>(c.rgb, c.a * quad.opacity * maskAlpha);
			}
		`;

		const layerQuadModule = device.createShaderModule({ code: layerQuadWgsl });
		this.layerQuadPipeline = device.createRenderPipeline({
			layout: "auto",
			vertex: {
				module: layerQuadModule,
				entryPoint: "vs_main",
			},
			fragment: {
				module: layerQuadModule,
				entryPoint: "fs_main",
				targets: [
					{
						format: this.state.preferredFormat,
						blend: {
							color: {
								srcFactor: "src-alpha",
								dstFactor: "one-minus-src-alpha",
								operation: "add",
							},
							alpha: {
								srcFactor: "one",
								dstFactor: "one-minus-src-alpha",
								operation: "add",
							},
						},
					},
				],
			},
			primitive: {
				topology: "triangle-strip",
			},
		});

		// 2. Color Grading Compute Shader
		const colorGradeWgsl = `
			struct ColorParams {
				brightness: f32,
				contrast: f32,
				saturation: f32,
				exposure: f32,
				temperature: f32,
				tint: f32,
				hue: f32,
				gamma: f32,
				width: u32,
				height: u32,
			};

			@group(0) @binding(0) var<uniform> params: ColorParams;
			@group(0) @binding(1) var input_tex: texture_2d<f32>;
			@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

			@compute @workgroup_size(16, 16)
			fn cs_main(@builtin(global_invocation_id) id: vec3<u32>) {
				if (id.x >= params.width || id.y >= params.height) { return; }
				let c = textureLoad(input_tex, vec2<i32>(id.xy), 0);
				let exp_mult = pow(2.0, params.exposure);
				var r = (c.r * exp_mult) + params.brightness;
				var g = (c.g * exp_mult) + params.brightness;
				var b = (c.b * exp_mult) + params.brightness;

				r = (r - 0.5) * (1.0 + params.contrast) + 0.5;
				g = (g - 0.5) * (1.0 + params.contrast) + 0.5;
				b = (b - 0.5) * (1.0 + params.contrast) + 0.5;

				r = r * (1.0 + params.temperature * 0.1);
				g = g * (1.0 + params.tint * 0.1);
				b = b * (1.0 - params.temperature * 0.1);

				let luma = 0.2126 * r + 0.7152 * g + 0.0722 * b;
				let sat_factor = 1.0 + params.saturation;
				r = luma + (r - luma) * sat_factor;
				g = luma + (g - luma) * sat_factor;
				b = luma + (b - luma) * sat_factor;

				if (abs(params.gamma - 1.0) > 0.001 && params.gamma > 0.0) {
					let inv_gamma = 1.0 / params.gamma;
					r = pow(max(r, 0.0), inv_gamma);
					g = pow(max(g, 0.0), inv_gamma);
					b = pow(max(b, 0.0), inv_gamma);
				}

				textureStore(output_tex, vec2<i32>(id.xy), vec4<f32>(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), c.a));
			}
		`;
		const colorGradeModule = device.createShaderModule({ code: colorGradeWgsl });
		this.colorGradePipeline = device.createComputePipeline({
			layout: "auto",
			compute: {
				module: colorGradeModule,
				entryPoint: "cs_main",
			},
		});

		// 3. Chroma Key Compute Shader
		const chromaKeyWgsl = `
			struct ChromaParams {
				key_color: vec3<f32>,
				similarity: f32,
				smoothness: f32,
				spill: f32,
				width: u32,
				height: u32,
			};

			@group(0) @binding(0) var<uniform> params: ChromaParams;
			@group(0) @binding(1) var input_tex: texture_2d<f32>;
			@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

			@compute @workgroup_size(16, 16)
			fn cs_main(@builtin(global_invocation_id) id: vec3<u32>) {
				if (id.x >= params.width || id.y >= params.height) { return; }
				var c = textureLoad(input_tex, vec2<i32>(id.xy), 0);
				let diff = c.rgb - params.key_color;
				let dist = length(diff);
				let sim = max(params.similarity, 0.01);
				let smooth = max(params.smoothness, 0.001);
				var alpha = clamp((dist - sim) / smooth, 0.0, 1.0);

				var r = c.r;
				var g = c.g;
				var b = c.b;
				if (params.spill > 0.0 && alpha < 1.0) {
					let max_non_key = max(r, b);
					let excess = max(0.0, g - max_non_key);
					g = g - excess * params.spill * (1.0 - alpha);
				}

				textureStore(output_tex, vec2<i32>(id.xy), vec4<f32>(r, g, b, c.a * alpha));
			}
		`;
		const chromaKeyModule = device.createShaderModule({ code: chromaKeyWgsl });
		this.chromaKeyPipeline = device.createComputePipeline({
			layout: "auto",
			compute: {
				module: chromaKeyModule,
				entryPoint: "cs_main",
			},
		});

		// 4. Separable Gaussian Blur Compute Shader
		const blurWgsl = `
			struct BlurParams {
				direction: vec2<f32>,
				sigma: f32,
				radius: u32,
				width: u32,
				height: u32,
			};

			@group(0) @binding(0) var<uniform> params: BlurParams;
			@group(0) @binding(1) var input_tex: texture_2d<f32>;
			@group(0) @binding(2) var output_tex: texture_storage_2d<rgba8unorm, write>;

			@compute @workgroup_size(16, 16)
			fn cs_main(@builtin(global_invocation_id) id: vec3<u32>) {
				if (id.x >= params.width || id.y >= params.height) { return; }
				let r = i32(params.radius);
				let s = params.sigma;
				var weight_sum = 0.0;
				var color_sum = vec4<f32>(0.0);

				for (var i = -r; i <= r; i = i + 1) {
					let offset = vec2<f32>(f32(i) * params.direction.x, f32(i) * params.direction.y);
					let sample_pos = vec2<i32>(
						clamp(i32(id.x) + i32(offset.x), 0, i32(params.width) - 1),
						clamp(i32(id.y) + i32(offset.y), 0, i32(params.height) - 1)
					);
					let weight = exp(-0.5 * (f32(i) * f32(i)) / (s * s));
					let c = textureLoad(input_tex, sample_pos, 0);
					color_sum = color_sum + c * weight;
					weight_sum = weight_sum + weight;
				}

				textureStore(output_tex, vec2<i32>(id.xy), color_sum / weight_sum);
			}
		`;
		const blurModule = device.createShaderModule({ code: blurWgsl });
		this.blurPipelineH = device.createComputePipeline({
			layout: "auto",
			compute: {
				module: blurModule,
				entryPoint: "cs_main",
			},
		});
		this.blurPipelineV = device.createComputePipeline({
			layout: "auto",
			compute: {
				module: blurModule,
				entryPoint: "cs_main",
			},
		});

		// 5. Timeline Audio Waveform Compute Shader
		const waveformWgsl = `
			struct WaveformParams {
				sample_count: u32,
				bucket_count: u32,
			};

			@group(0) @binding(0) var<uniform> params: WaveformParams;
			@group(0) @binding(1) var<storage, read> samples: array<f32>;
			@group(0) @binding(2) var<storage, read_write> peaks: array<vec2<f32>>;

			@compute @workgroup_size(64)
			fn cs_main(@builtin(global_invocation_id) id: vec3<u32>) {
				let bucket = id.x;
				if (bucket >= params.bucket_count) { return; }

				let step = max(1u, params.sample_count / params.bucket_count);
				let start_idx = bucket * step;
				let end_idx = min(start_idx + step, params.sample_count);

				var min_val = 0.0;
				var max_val = 0.0;

				for (var i = start_idx; i < end_idx; i = i + 1u) {
					let s = samples[i];
					min_val = min(min_val, s);
					max_val = max(max_val, s);
				}

				peaks[bucket] = vec2<f32>(min_val, max_val);
			}
		`;
		const waveformModule = device.createShaderModule({ code: waveformWgsl });
		this.waveformPipeline = device.createComputePipeline({
			layout: "auto",
			compute: {
				module: waveformModule,
				entryPoint: "cs_main",
			},
		});
	}

	acquireTexture({
		width,
		height,
		format = "rgba8unorm",
		usage,
	}: {
		width: number;
		height: number;
		format?: GPUTextureFormat;
		usage: GPUTextureUsageFlags;
	}): GPUTexture {
		const device = this.state.device;
		if (!device) {
			throw new Error("WebGPU device not available");
		}

		// Look for compatible pooled texture
		const index = this.texturePool.findIndex(
			(e) => e.width === width && e.height === height && e.format === format && (e.texture.usage & usage) === usage,
		);

		if (index !== -1) {
			const entry = this.texturePool.splice(index, 1)[0];
			entry.lastUsed = Date.now();
			return entry.texture;
		}

		return device.createTexture({
			size: [width, height],
			format,
			usage,
		});
	}

	releaseTexture(texture: GPUTexture): void {
		if (this.texturePool.length >= this.maxPoolSize) {
			const oldest = this.texturePool.shift();
			oldest?.texture.destroy();
		}

		this.texturePool.push({
			texture,
			width: texture.width,
			height: texture.height,
			format: texture.format,
			lastUsed: Date.now(),
		});
	}

	private layerUniformBuffers: GPUBuffer[] = [];
	beginEffectsFrame() { this.effectBufferIndex = 0; }
	applyBlurPasses(source: GPUTexture, passes: Array<{ shader: string; uniforms: Record<string, number | number[]> }>, temporaries: GPUTexture[]): GPUTexture {
		const device = this.state.device;
		if (!device || !this.blurPipelineH) return source;
		let input = source;
		for (const pass of passes) {
			if (pass.shader !== "gaussian-blur") continue;
			const sigma = Number(pass.uniforms.u_sigma ?? 0);
			if (sigma < .001) continue;
			const direction = pass.uniforms.u_direction as number[] ?? [1, 0];
			const step = Number(pass.uniforms.u_step ?? 1);
			const bytes = new ArrayBuffer(32); const floats = new Float32Array(bytes); const integers = new Uint32Array(bytes);
			floats[0] = direction[0] * step; floats[1] = direction[1] * step; floats[2] = sigma;
			integers[3] = Math.min(40, Math.ceil(sigma * 3)); integers[4] = source.width; integers[5] = source.height;
			const index = this.effectBufferIndex++;
			const buffer = this.effectBuffers[index] ??= device.createBuffer({ size: 32, usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST });
			device.queue.writeBuffer(buffer, 0, bytes);
			const output = this.acquireTexture({ width: source.width, height: source.height, usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.STORAGE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT });
			temporaries.push(output);
			const group = device.createBindGroup({ layout: this.blurPipelineH.getBindGroupLayout(0), entries: [
				{ binding: 0, resource: { buffer } }, { binding: 1, resource: input.createView() }, { binding: 2, resource: output.createView() },
			] });
			const encoder = device.createCommandEncoder(); const compute = encoder.beginComputePass();
			compute.setPipeline(this.blurPipelineH); compute.setBindGroup(0, group); compute.dispatchWorkgroups(Math.ceil(source.width / 16), Math.ceil(source.height / 16)); compute.end();
			device.queue.submit([encoder.finish()]); input = output;
		}
		return input;
	}

	private getLayerUniformBuffer(index: number): GPUBuffer {
		const device = this.state.device!;
		while (this.layerUniformBuffers.length <= index) {
			const buf = device.createBuffer({
				size: 64,
				usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
			});
			this.layerUniformBuffers.push(buf);
		}
		return this.layerUniformBuffers[index];
	}

	uploadImageToTexture(image: ImageBitmap | HTMLVideoElement | HTMLCanvasElement | OffscreenCanvas): GPUTexture | null {
		const device = this.state.device;
		if (!device) return null;

		const width = (image as any).videoWidth || (image as any).width;
		const height = (image as any).videoHeight || (image as any).height;
		if (!width || !height || width <= 0 || height <= 0) return null;

		if (typeof HTMLVideoElement !== "undefined" && image instanceof HTMLVideoElement && image.readyState < 2) {
			return null;
		}

		const texture = this.acquireTexture({
			width,
			height,
			format: "rgba8unorm",
			usage: GPUTextureUsage.TEXTURE_BINDING | GPUTextureUsage.COPY_DST | GPUTextureUsage.RENDER_ATTACHMENT | GPUTextureUsage.STORAGE_BINDING,
		});

		try {
			device.queue.copyExternalImageToTexture(
				{ source: image },
				{ texture },
				[width, height],
			);
		} catch (err) {
			console.warn("[WebGPU] copyExternalImageToTexture failed:", err);
			this.releaseTexture(texture);
			return null;
		}

		return texture;
	}

	updateTextureFromImage(
		texture: GPUTexture,
		image: ImageBitmap | HTMLVideoElement | HTMLCanvasElement | OffscreenCanvas,
		width: number,
		height: number,
	): boolean {
		const device = this.state.device;
		if (!device || !texture) return false;

		if (typeof HTMLVideoElement !== "undefined" && image instanceof HTMLVideoElement && image.readyState < 2) {
			return false;
		}

		try {
			device.queue.copyExternalImageToTexture(
				{ source: image },
				{ texture },
				[width, height],
			);
			return true;
		} catch (err) {
			console.warn("[WebGPU] updateTextureFromImage failed:", err);
			return false;
		}
	}

	renderTextureToCanvas({
		texture,
		targetCanvas,
	}: {
		texture: GPUTexture;
		targetCanvas: HTMLCanvasElement;
	}): boolean {
		const device = this.state.device;
		if (!device || !this.compositorPipeline || !this.sampler) return false;

		const ctx = (targetCanvas as any).getContext("webgpu") as GPUCanvasContext | null;
		if (!ctx) return false;

		ctx.configure({
			device,
			format: this.state.preferredFormat,
			alphaMode: "premultiplied",
		});

		const currentTexture = ctx.getCurrentTexture();
		const renderPassDescriptor: GPURenderPassDescriptor = {
			colorAttachments: [
				{
					view: currentTexture.createView(),
					clearValue: { r: 0.0, g: 0.0, b: 0.0, a: 1.0 },
					loadOp: "clear",
					storeOp: "store",
				},
			],
		};

		const bindGroup = device.createBindGroup({
			layout: this.compositorPipeline.getBindGroupLayout(0),
			entries: [
				{ binding: 0, resource: texture.createView() },
				{ binding: 1, resource: this.sampler },
			],
		});

		const commandEncoder = device.createCommandEncoder();
		const passEncoder = commandEncoder.beginRenderPass(renderPassDescriptor);
		passEncoder.setPipeline(this.compositorPipeline);
		passEncoder.setBindGroup(0, bindGroup);
		passEncoder.draw(4);
		passEncoder.end();

		device.queue.submit([commandEncoder.finish()]);
		return true;
	}

	renderFrameLayers({
		context,
		width,
		height,
		clearColor = [0, 0, 0, 1],
		layers,
		targetTexture,
	}: {
		context: GPUCanvasContext;
		width: number;
		height: number;
		clearColor?: [number, number, number, number];
		targetTexture?: GPUTexture;
		layers: Array<{
			texture: GPUTexture;
			maskTexture?: GPUTexture;
			maskInverted?: boolean;
			opacity?: number;
			transform?: {
				centerX: number;
				centerY: number;
				width: number;
				height: number;
				rotationDegrees?: number;
				flipX?: boolean;
				flipY?: boolean;
			};
		}>;
	}): boolean {
		const device = this.state.device;
		if (!device || !this.layerQuadPipeline || !this.sampler) return false;

		const currentTexture = targetTexture ?? context.getCurrentTexture();
		const commandEncoder = device.createCommandEncoder();

		const renderPass = commandEncoder.beginRenderPass({
			colorAttachments: [
				{
					view: currentTexture.createView(),
					clearValue: { r: clearColor[0], g: clearColor[1], b: clearColor[2], a: clearColor[3] },
					loadOp: "clear",
					storeOp: "store",
				},
			],
		});

		renderPass.setPipeline(this.layerQuadPipeline);

		for (let layerIndex = 0; layerIndex < layers.length; layerIndex++) {
			const layer = layers[layerIndex];
			const transform = layer.transform;
			let ndc: number[];
			if (!transform) {
				ndc = [-1.0, 1.0, 1.0, 1.0, -1.0, -1.0, 1.0, -1.0];
			} else {
				const cx = transform.centerX;
				const cy = transform.centerY;
				const tw = transform.width;
				const th = transform.height;
				const rad = ((transform.rotationDegrees || 0) * Math.PI) / 180;
				const cos = Math.cos(rad);
				const sin = Math.sin(rad);
				const sx = transform.flipX ? -1 : 1;
				const sy = transform.flipY ? -1 : 1;

				const localCorners = [
					[-tw / 2 * sx, -th / 2 * sy],
					[ tw / 2 * sx, -th / 2 * sy],
					[-tw / 2 * sx,  th / 2 * sy],
					[ tw / 2 * sx,  th / 2 * sy],
				];

				ndc = [];
				for (const [lx, ly] of localCorners) {
					const px = cx + lx * cos - ly * sin;
					const py = cy + lx * sin + ly * cos;
					const ndcX = (px / width) * 2.0 - 1.0;
					const ndcY = 1.0 - (py / height) * 2.0;
					ndc.push(ndcX, ndcY);
				}
			}

			const uniformData = new Float32Array([
				ndc[0], ndc[1],
				ndc[2], ndc[3],
				ndc[4], ndc[5],
				ndc[6], ndc[7],
				layer.opacity ?? 1.0,
				0, 0, 0,
				width, height, layer.maskInverted ? 2 : layer.maskTexture ? 1 : 0, 0,
			]);

			const uniformBuffer = this.getLayerUniformBuffer(layerIndex);
			device.queue.writeBuffer(uniformBuffer, 0, uniformData.buffer, uniformData.byteOffset, uniformData.byteLength);

			const bindGroup = device.createBindGroup({
				layout: this.layerQuadPipeline.getBindGroupLayout(0),
				entries: [
					{ binding: 0, resource: layer.texture.createView() },
					{ binding: 1, resource: this.sampler },
					{ binding: 2, resource: { buffer: uniformBuffer } },
					{ binding: 3, resource: (layer.maskTexture ?? this.whiteTexture!).createView() },
				],
			});

			renderPass.setBindGroup(0, bindGroup);
			renderPass.draw(4);
		}

		renderPass.end();
		device.queue.submit([commandEncoder.finish()]);
		return true;
	}

	async computeAudioWaveformPeaks({
		samples,
		numBuckets,
	}: {
		samples: Float32Array;
		numBuckets: number;
	}): Promise<Float32Array> {
		const device = this.state.device;
		if (!device || !this.waveformPipeline || numBuckets <= 0 || samples.length === 0) {
			// CPU/TypedArray fallback
			const peaks = new Float32Array(numBuckets * 2);
			const step = Math.max(1, Math.floor(samples.length / numBuckets));
			for (let b = 0; b < numBuckets; b++) {
				const start = b * step;
				const end = Math.min(start + step, samples.length);
				let minVal = 0;
				let maxVal = 0;
				for (let i = start; i < end; i++) {
					const s = samples[i];
					if (s < minVal) minVal = s;
					if (s > maxVal) maxVal = s;
				}
				peaks[b * 2] = minVal;
				peaks[b * 2 + 1] = maxVal;
			}
			return peaks;
		}

		try {
			// 1. Uniforms
			const uniformData = new Uint32Array([samples.length, numBuckets]);
			const uniformBuffer = device.createBuffer({
				size: 16,
				usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST,
			});
			device.queue.writeBuffer(uniformBuffer, 0, uniformData.buffer);

			// 2. Samples Storage Buffer
			const samplesBuffer = device.createBuffer({
				size: Math.max(16, samples.byteLength),
				usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST,
			});
			device.queue.writeBuffer(samplesBuffer, 0, samples.buffer, samples.byteOffset, samples.byteLength);

			// 3. Output Peaks Storage Buffer
			const peaksByteLength = numBuckets * 2 * 4;
			const peaksBuffer = device.createBuffer({
				size: Math.max(16, peaksByteLength),
				usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC,
			});

			const readBuffer = device.createBuffer({
				size: Math.max(16, peaksByteLength),
				usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST,
			});

			// 4. Bind Group & Compute Pass
			const bindGroup = device.createBindGroup({
				layout: this.waveformPipeline.getBindGroupLayout(0),
				entries: [
					{ binding: 0, resource: { buffer: uniformBuffer } },
					{ binding: 1, resource: { buffer: samplesBuffer } },
					{ binding: 2, resource: { buffer: peaksBuffer } },
				],
			});

			const commandEncoder = device.createCommandEncoder();
			const pass = commandEncoder.beginComputePass();
			pass.setPipeline(this.waveformPipeline);
			pass.setBindGroup(0, bindGroup);
			const workgroups = Math.ceil(numBuckets / 64);
			pass.dispatchWorkgroups(workgroups);
			pass.end();

			commandEncoder.copyBufferToBuffer(peaksBuffer, 0, readBuffer, 0, peaksByteLength);
			device.queue.submit([commandEncoder.finish()]);

			await readBuffer.mapAsync(GPUMapMode.READ);
			const copy = new Float32Array(readBuffer.getMappedRange()).slice(0, numBuckets * 2);
			readBuffer.unmap();

			uniformBuffer.destroy();
			samplesBuffer.destroy();
			peaksBuffer.destroy();
			readBuffer.destroy();

			return copy;
		} catch (err) {
			console.warn("[WebGPU] Audio waveform compute error, fallback to CPU:", err);
			const peaks = new Float32Array(numBuckets * 2);
			return peaks;
		}
	}
}

export const webGpuEngine = new WebGpuEngine();
