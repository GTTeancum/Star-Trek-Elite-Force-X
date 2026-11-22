// Included by the Xbox D3D backend. Each MDR weight has its own bone-local
// offset, so one shared bind-position attribute cannot represent this format.
#include "stefx_gpu_skin_programs.h"

static const int actorPaletteLimit = 29; // c8 through c94, three rows per bone
static const unsigned actorBufferBudget = 768 * 1024;
static const unsigned actorFreeReserve = 512 * 1024;

struct splitActorMesh_t {
    const md4Surface_t *surface;
    IDirect3DVertexBuffer8 *vertices;
    int weights, stride, boneCount;
    byte bones[actorPaletteLimit];
};

static splitActorMesh_t actorMeshes[128];
static unsigned actorMeshCount, actorBufferBytes;
static DWORD actorPrograms[5];
static splitActorMesh_t *activeActor;
static const md4Bone_t *activePalette;
static cvar_t *actorSkinMode;
extern "C" { volatile unsigned int g_SPXBGpuSkin[12]; }

void STEFX_ResetActorMeshes() {
    if (!actorMeshCount) return;
    if (actorBufferBytes && glw_state && glw_state->device)
        glw_state->device->BlockUntilIdle();
    for (unsigned i = 0; i < actorMeshCount; ++i) {
        if (actorMeshes[i].vertices) actorMeshes[i].vertices->Release();
    }
    memset(actorMeshes, 0, sizeof(actorMeshes));
    actorMeshCount = actorBufferBytes = 0;
    activeActor = NULL;
    activePalette = NULL;
}

static void STEFX_ShutdownActorMeshes() {
    STEFX_ResetActorMeshes();
    for (int width = 1; width <= 4; ++width) {
        if (actorPrograms[width]) {
            glw_state->device->DeleteVertexShader(actorPrograms[width]);
            actorPrograms[width] = 0;
        }
    }
}

static bool STEFX_CreateActorProgram(int width) {
    if (actorPrograms[width]) return true;
    DWORD declaration[] = {
        D3DVSD_STREAM(0), D3DVSD_REG(0, D3DVSDT_FLOAT3), D3DVSD_REG(1, D3DVSDT_FLOAT2),
        D3DVSD_REG(2, D3DVSDT_FLOAT4), D3DVSD_REG(3, D3DVSDT_FLOAT4), D3DVSD_REG(4, D3DVSDT_FLOAT4),
        D3DVSD_REG(5, D3DVSDT_FLOAT4), D3DVSD_REG(6, D3DVSDT_FLOAT4), D3DVSD_END()
    };
    declaration[4 + width] = D3DVSD_END();
    const DWORD *programs[] = { NULL, gpuSkinProgram1, gpuSkinProgram2, gpuSkinProgram3, gpuSkinProgram4 };
    return SUCCEEDED(glw_state->device->CreateVertexShader(declaration, programs[width], &actorPrograms[width], 0));
}

static void STEFX_CreateActorMesh(splitActorMesh_t &mesh, const md4Surface_t *surface, int modelBones) {
    mesh.surface = surface; // Cache unsupported meshes as well.
    if (modelBones < 1 || modelBones > MD4_MAX_BONES || surface->numVerts < 1) return;
    int local[MD4_MAX_BONES];
    memset(local, -1, sizeof(local));
    const md4Vertex_t *first = (const md4Vertex_t *)((const byte *)surface + surface->ofsVerts);
    const md4Vertex_t *vertex = first;
    for (int i = 0; i < surface->numVerts; ++i) {
        if (vertex->numWeights < 1 || vertex->numWeights > 4) return;
        if (mesh.weights < vertex->numWeights) mesh.weights = vertex->numWeights;
        for (int j = 0; j < vertex->numWeights; ++j) {
            const int bone = vertex->weights[j].boneIndex;
            if (bone < 0 || bone >= modelBones) return;
            if (local[bone] < 0) {
                if (mesh.boneCount == actorPaletteLimit) return;
                local[bone] = mesh.boneCount;
                mesh.bones[mesh.boneCount++] = (byte)bone;
            }
        }
        vertex = (const md4Vertex_t *)&vertex->weights[vertex->numWeights];
    }

    mesh.stride = sizeof(float) * (9 + 4 * mesh.weights);
    const unsigned bytes = mesh.stride * surface->numVerts;
    MEMORYSTATUS memory;
    GlobalMemoryStatus(&memory);
    if (bytes > actorBufferBudget - actorBufferBytes || memory.dwAvailPhys < bytes + actorFreeReserve) return;
    if (!STEFX_CreateActorProgram(mesh.weights)) return;

    IDirect3DVertexBuffer8 *buffer = NULL;
    if (FAILED(glw_state->device->CreateVertexBuffer(bytes, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &buffer))) return;
    BYTE *data = NULL;
    if (FAILED(buffer->Lock(0, bytes, &data, 0)) || !data) {
        buffer->Release();
        return;
    }
    memset(data, 0, bytes);
    vertex = first;
    for (int i = 0; i < surface->numVerts; ++i) {
        float *stream = (float *)(data + i * mesh.stride);
        memcpy(stream, vertex->normal, sizeof(vec3_t));
        memcpy(stream + 3, vertex->texCoords, sizeof(vec2_t));
        for (int j = 0; j < mesh.weights; ++j) {
            stream[5 + j] = 8.0f; // Zero-weight padding still needs a valid register address.
            if (j >= vertex->numWeights) continue;
            const md4Weight_t &weight = vertex->weights[j];
            stream[5 + j] = (float)(8 + 3 * local[weight.boneIndex]);
            float *offset = stream + 9 + j * 4;
            VectorScale(weight.offset, weight.boneWeight, offset);
            offset[3] = weight.boneWeight;
        }
        vertex = (const md4Vertex_t *)&vertex->weights[vertex->numWeights];
    }
    buffer->Unlock();
    mesh.vertices = buffer;
    actorBufferBytes += bytes;
    g_SPXBGpuSkin[5] = actorBufferBytes;
}

static bool STEFX_ActorMaterialSupported() {
    const shader_t *shader = tess.shader;
    if (!tess.xstages || !backEnd.currentEntity || shader->sort != SS_OPAQUE ||
        shader->numUnfoggedPasses != 1 || shader->numDeforms || shader->sky || shader->polygonOffset ||
        shader->remappedShader || shader == tr.projectionShadowShader || shader == tr.shadowShader ||
        shader == tr.distortionShader || tess.fogNum || tess.fading || tess.dlightBits ||
        backEnd.projection2D || backEnd.viewParms.isPortal || backEnd.currentEntity->e.nonNormalizedAxes ||
        r_showtris->integer || r_shownormals->integer) return false;
    int special = RF_RGB_TINT | RF_DISINTEGRATE1 | RF_DISINTEGRATE2 | RF_VOLUMETRIC |
        RF_DISTORTION | RF_DEPTHHACK | RF_NODEPTH;
#ifdef STEFX_SP_HOSTED_MP
    special |= RF_STEFX_FORCE_ENT_ALPHA;
#endif
    if (backEnd.currentEntity->e.renderfx & special) return false;
    const shaderStage_t &stage = tess.xstages[0];
    return stage.active && stage.bundle[0].image && !stage.bundle[1].image &&
        stage.bundle[0].tcGen == TCGEN_TEXTURE && !stage.bundle[0].numTexMods &&
        !stage.isEnvironment && !stage.isBumpMap && !stage.ss &&
        !(stage.stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) &&
        (stage.rgbGen == CGEN_LIGHTING_DIFFUSE || stage.rgbGen == CGEN_IDENTITY) &&
        (stage.alphaGen == AGEN_IDENTITY || stage.alphaGen == AGEN_SKIP);
}

bool STEFX_PrepareActorMesh(md4Surface_t *surface, const md4Bone_t *bones, int modelBones) {
    if (!actorSkinMode) actorSkinMode = Cvar_Get("r_efGpuSkin", "0", 0);
    if (!actorSkinMode->integer || !STEFX_ActorMaterialSupported()) return false;
    unsigned i = 0;
    while (i < actorMeshCount && actorMeshes[i].surface != surface) ++i;
    if (i == actorMeshCount) {
        if (actorMeshCount == sizeof(actorMeshes) / sizeof(actorMeshes[0])) return false;
        ++actorMeshCount;
        STEFX_CreateActorMesh(actorMeshes[i], surface, modelBones);
    }
    if (!actorMeshes[i].vertices) return false;
    activeActor = &actorMeshes[i];
    activePalette = bones;
    return true;
}

void STEFX_FinishActorMesh() {
    activeActor = NULL;
    activePalette = NULL;
}

static void STEFX_RestoreActorVertices() {
    const md4Surface_t *surface = activeActor->surface;
    const md4Vertex_t *vertex = (const md4Vertex_t *)((const byte *)surface + surface->ofsVerts);
    for (int i = 0; i < surface->numVerts; ++i) {
        VectorClear(tess.xyz[i]);
        VectorClear(tess.normal[i]);
        for (int j = 0; j < vertex->numWeights; ++j) {
            const md4Weight_t &weight = vertex->weights[j];
            const md4Bone_t &bone = activePalette[weight.boneIndex];
            for (int axis = 0; axis < 3; ++axis) {
                tess.xyz[i][axis] += weight.boneWeight * (DotProduct(bone.matrix[axis], weight.offset) + bone.matrix[axis][3]);
                tess.normal[i][axis] += weight.boneWeight * DotProduct(bone.matrix[axis], vertex->normal);
            }
        }
        memcpy(tess.texCoords[i][0], vertex->texCoords, sizeof(vec2_t));
        memcpy(tess.svars.texcoords[0][i], vertex->texCoords, sizeof(vec2_t));
        vertex = (const md4Vertex_t *)&vertex->weights[vertex->numWeights];
    }
}

static bool STEFX_DrawActorMesh(int count, const WORD *indices) {
    const splitActorMesh_t &mesh = *activeActor;
    D3DXMATRIX composite, transposed;
    D3DXMatrixMultiply(&composite, glw_state->matrixStack[glwstate_t::MatrixMode_Model]->GetTop(),
        glw_state->matrixStack[glwstate_t::MatrixMode_Projection]->GetTop());
    D3DXMatrixTranspose(&transposed, &composite);
    float light[3][4] = {{0,1,0,0}, {0,0,0,0}, {1,1,1,1}};
    if (tess.xstages[0].rgbGen == CGEN_LIGHTING_DIFFUSE) {
        memcpy(light[0], backEnd.currentEntity->lightDir, sizeof(vec3_t));
        VectorNormalize(light[0]);
        for (int i = 0; i < 3; ++i) {
            light[1][i] = backEnd.currentEntity->directedLight[i] / 255.0f;
            light[2][i] = backEnd.currentEntity->ambientLight[i] / 255.0f;
        }
    }
    md4Bone_t palette[actorPaletteLimit];
    for (int i = 0; i < mesh.boneCount; ++i) palette[i] = activePalette[mesh.bones[i]];
    const DWORD oldShader = glw_state->shaderMask;
    const UINT oldStride = glw_state->streamSourceZeroStride;
    IDirect3DDevice8 *device = glw_state->device;
    HRESULT result = STEFX_D3D8_SetVertexShaderTracked(actorPrograms[mesh.weights]);
    if (SUCCEEDED(result)) result = device->SetVertexShaderConstant(0, &transposed, 4);
    if (SUCCEEDED(result)) result = device->SetVertexShaderConstant(4, light, 3);
    if (SUCCEEDED(result)) result = device->SetVertexShaderConstant(8, palette, 3 * mesh.boneCount);
    if (SUCCEEDED(result)) result = device->SetStreamSource(0, mesh.vertices, mesh.stride);
    if (SUCCEEDED(result)) result = device->DrawIndexedVertices(D3DPT_TRIANGLELIST, count, indices);
    glw_state->streamSourceZeroValid = false;
    STEFX_D3D8_SetStreamSourceZeroTracked(oldStride);
    STEFX_D3D8_SetVertexShaderTracked(oldShader);
    if (FAILED(result)) {
        XBLog_WriteCriticalf("STEFX_ACTOR_GPU: draw failed hr=0x%x, returning to CPU", result);
        actorSkinMode->integer = 0;
        STEFX_RestoreActorVertices();
        ++g_SPXBGpuSkin[9];
        return false;
    }
    if (!g_SPXBGpuSkin[3]) XBLog_WriteCritical("STEFX_ACTOR_GPU: submitting split-screen MDR weights on NV2A");
    ++g_SPXBGpuSkin[3];
    g_SPXBGpuSkin[4] += tess.numVertexes;
    return true;
}
