#include "qdata.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAX_MDXSKINS 32
#define MAX_GLCMDS 4096

int mdx_numxyz;
int mdx_numtris;
int mdx_commands[16384];
int mdx_xyz[8192];
int mdx_numcommands;
int mdx_numsfxobj;

char g_skins[MAX_MDXSKINS][MAX_SKINNAME];
dmdxl_t model_mdx;
dxtriangle_t mdx_triangles[MAX_TRIANGLES];
int mdx_glcmds[MAX_GLCMDS];


int MD2_to_MDX(const void* md2_data)
{
    dmdl_t* md2_model = (const dmdl_t*)md2_data;

    // Handle skins
    if (mdx_numxyz == 0) {
        md2_model->num_skins = 0;
    } else {
        const char* skin_names = (const char*)((uintptr_t)md2_data + md2_model->ofs_skins);
        for (int j = 0; j < md2_model->num_skins; ++j) {
            strcpy(g_skins[j], skin_names + j * MAX_SKINNAME);
        }
        model_mdx.num_skins = md2_model->num_skins;
        model_mdx.skinwidth = md2_model->skinwidth;
        model_mdx.skinheight = md2_model->skinheight;
    }

    // Convert triangles
    const dtriangle_t* md2_triangles = (const dtriangle_t*)((uintptr_t)md2_data + md2_model->ofs_tris);
    for (int k = 0; k < md2_model->num_tris; ++k) {
        for (int l = 0; l < 3; ++l) {
            mdx_triangles[mdx_numtris + k].index_xyz[l] =
                mdx_numxyz + LittleShort(md2_triangles[k].index_xyz[l]);
            mdx_triangles[mdx_numtris + k].index_xyz[l + 3] =
                mdx_numxyz + LittleShort(md2_triangles[k].index_st[l]);
        }
    }
    mdx_numtris += md2_model->num_tris;

    // Handle frames
    if (model_mdx.num_frames && (model_mdx.num_frames != md2_model->num_frames)) {
        fprintf(stderr, "Frame count mismatch for MDX command\n");
        return 2; // Error: Frame count mismatch
    } else {
        model_mdx.num_frames = md2_model->num_frames;
    }

    const char* frame_data = (const char*)((uintptr_t)md2_data + md2_model->ofs_frames);
    for (int m = 0; m < md2_model->num_frames; ++m) {
        const daliasframe_t* frame = (const daliasframe_t*)(frame_data + m * md2_model->framesize);
        // ... (Process frame data: copy positions, normals, etc.) ...
    }

    // Convert GL commands
    const int* gl_commands = (const int*)((uintptr_t)md2_data + md2_model->ofs_glcmds);
    for (int kk = 0; kk < md2_model->num_glcmds; ++kk) {
        mdx_glcmds[mdx_numcommands + kk] = LittleLong(gl_commands[kk]);
    }

    // Update counters
    mdx_numxyz += md2_model->num_xyz;
    mdx_numcommands += md2_model->num_glcmds;

    return 0; // Conversion successful
}

int KP_filelength(FILE *f)
{
	int		pos;
	int		end;

	pos = ftell (f);
	fseek (f, 0, SEEK_END);
	end = ftell (f);
	fseek (f, pos, SEEK_SET);

	return end;
}

int LoadMD2File (char *filename)
{
    int length; // ST5C_4
    char file[256]; // [esp+50h] [ebp-84h]
    FILE *f; // [esp+D0h] [ebp-4h]
    unsigned char* mdxOut_buff; // Declare mdxOut_buff here

    strcpy(&file, filename);
    strcat(&file, ".md2");

    f = fopen(&file, "rb");
    if (!f)
    {
        fprintf(stderr, "reader: could not open file '%s'\n", &file);
        exit(0);
    }

    length = KP_filelength(f);

    // Allocate memory for storing the MDX data
    mdxOut_buff = malloc(2 * length);

    // Read the contents of the file into this buffer
    fread(mdxOut_buff, 1, length, f);

    // Convert the MDX data (custom function)
    MD2_to_MDX((int)mdxOut_buff);

    // Free the allocated memory
    free(mdxOut_buff);

    // Close the opened file
    return fclose(f);
}

//=================================================================

typedef struct
{
	int		numnormals;
	vec3_t	normalsum;
} vertexnormals_t;

typedef struct
{
	vec3_t		v;
	int			lightnormalindex;
} trivert_t;

typedef struct
{
	vec3_t		mins, maxs;
	char		name[16];
	trivert_t	v[MAX_VERTS];
} frame_t;

//================================================================

frame_t		g_frames[MAX_FRAMES];

void WriteMDXModelFile(char *filename)
{
	int				i;
    int fr_sfx;
    int frames;
    int temp;
    vec_t vertexValue;
    double vertexRounded;
    double floatVal1, floatVal2;
    int offset = 0;
    float adjustedVertex;

	int				j, k;
	frame_t			*in;
	daliasframe_t	*out;
	byte			mdx_buffer[MAX_VERTS*4+128];
    float			v;

    int frameIndex = 0;
    char file[256];
    FILE *modelouthandle;
    dmdxl_t modelTemp;

    // Construct the filename with .mdx extension
    strcpy(file, filename);
    strcat(file, ".mdx");

    // Open the file in binary write mode
    modelouthandle = fopen(file, "wb");
    if (!modelouthandle)
    {
        fprintf(stderr, "MDX write: could not open file '%s'\n", file);
        exit(0);
    }

    // Set MDX model properties
    model_mdx.num_xyz = mdx_numxyz;
    model_mdx.num_tris = mdx_numtris;
    model_mdx.num_glcmds = mdx_numcommands;
    model_mdx.num_sfxdef = 0;
    model_mdx.num_sfxent = 0;
    model_mdx.num_sfxobj = mdx_numsfxobj;
    model_mdx.ident = IDMDXHEADER;
    model_mdx.version = MDX_VERSION;
    model_mdx.framesize = 4 * mdx_numxyz + 40;

    model_mdx.ofs_skins = sizeof(dmdxl_t);
    model_mdx.ofs_tris = model_mdx.ofs_skins + (model_mdx.num_skins << 6);
    model_mdx.ofs_frames = model_mdx.ofs_tris + (12 * mdx_numtris);
    model_mdx.ofs_glcmds = model_mdx.ofs_frames + (model_mdx.framesize * model_mdx.num_frames);
    model_mdx.ofs_vertexi = model_mdx.ofs_glcmds + (4 * mdx_numcommands);
    model_mdx.ofs_sfxdef = model_mdx.ofs_vertexi + (4 * mdx_numxyz);
    model_mdx.ofs_sfxent = model_mdx.ofs_sfxdef;
    model_mdx.ofs_sfxbbox = model_mdx.ofs_sfxent;
    fr_sfx = 24 * model_mdx.num_frames * mdx_numsfxobj;
    frames = model_mdx.ofs_sfxbbox + fr_sfx;
    model_mdx.ofs_end = frames;
    model_mdx.ofs_dummyend = frames;

    // Convert and write model header
    for (i = 0; i < 23; ++i)
    {
        temp = LittleLong(*((int *)&model_mdx + i));
        *((int *)&modelTemp + i) = temp;
    }
    SafeWrite(modelouthandle, &modelTemp, sizeof(dmdxl_t));
    offset += sizeof(dmdxl_t);

    // Write skin data
    model_mdx.ofs_skins = offset;
    SafeWrite(modelouthandle, g_skins, model_mdx.num_skins << 6);
    offset += model_mdx.num_skins << 6;

	//
	// write out the triangles
	//
    model_mdx.ofs_tris = offset;
    for (i = 0; i < model_mdx.num_tris; i++)
	{
		int			j;
		dtriangle_t	tri;

		for (j=0 ; j<3 ; j++)
		{
  			tri.index_xyz[j] = LittleShort (mdx_triangles[i].index_xyz[j]);
			tri.index_st[j] = LittleShort (mdx_triangles[i].index_st[j]);
        }
        SafeWrite(modelouthandle, &tri, sizeof(dxtriangle_t));
        offset += sizeof(dxtriangle_t);
    }

	//
	// write out the frames
	//
    model_mdx.ofs_frames = offset;
    for (i = 0; i < model_mdx.num_frames; i++)
	{
		in = &g_frames[i];
        out = (daliasframe_t *)mdx_buffer;

		strcpy (out->name, in->name);
		for (j=0 ; j<3 ; j++)
		{
			out->scale[j] = (in->maxs[j] - in->mins[j])/255;
			out->translate[j] = in->mins[j];
		}

        for (j = 0; j < model_mdx.num_xyz ; j++)
		{
		// all of these are byte values, so no need to deal with endianness
			out->verts[j].lightnormalindex = in->v[j].lightnormalindex;

			for (k=0 ; k<3 ; k++)
			{
			// scale to byte values & min/max check
				v = Q_rint ( (in->v[j].v[k] - out->translate[k]) / out->scale[k] );

			// clamp, so rounding doesn't wrap from 255.6 to 0
				if (v > 255.0)
					v = 255.0;
				if (v < 0)
					v = 0;
				out->verts[j].v[k] = v;
			}
		}

		for (j=0 ; j<3 ; j++)
		{
			out->scale[j] = LittleFloat (out->scale[j]);
			out->translate[j] = LittleFloat (out->translate[j]);
		}
        SafeWrite(modelouthandle, out, model_mdx.framesize);
        offset += model_mdx.framesize;
    }

	//
	// write out glcmds
	//
    model_mdx.ofs_glcmds = offset;
    SafeWrite(modelouthandle, mdx_commands, mdx_numcommands*4);
    offset += 4 * mdx_numcommands;

    // Write vertex data
    model_mdx.ofs_vertexi = offset;
    SafeWrite(modelouthandle, mdx_xyz, 4 * mdx_numxyz);
    offset += 4 * mdx_numxyz;

    // Write SFX data
    model_mdx.ofs_sfxdef = offset;
    model_mdx.ofs_sfxent = offset;
    model_mdx.ofs_sfxbbox = offset;

    /*//FREDZ need fix
    for (i = 0; i < model_mdx.num_sfxobj; ++i)
    {
        SafeWrite(modelouthandle, (char *)&unk_1EA5C40 + 12288 * i, 24 * model_mdx.num_frames);
        offset += 24 * model_mdx.num_frames;
    }*/

    // Finalize model data
    model_mdx.ofs_end = offset;
    printf("%3dx%3d skin\n", model_mdx.skinwidth, model_mdx.skinheight);
    printf("%4d vertexes\n", model_mdx.num_xyz);
    printf("%4d triangles\n", model_mdx.num_tris);
    printf("%4d frames\n", model_mdx.num_frames);
    printf("%4d glcmds\n", model_mdx.num_glcmds);
    printf("%4d skins\n", model_mdx.num_skins);
    printf("%4d sfx defines\n", model_mdx.num_sfxdef);
    printf("%4d sfx entries\n", model_mdx.num_sfxent);
    printf("%4d sub-objects\n", mdx_numsfxobj);
    printf("file size: %d\n", ftell(modelouthandle));
    printf("---------------------\n");
    fclose(modelouthandle);
}


void Cmd_MDX(void)
{
    char filename[1024]; // Buffer for MDX file name
    int mode; // Mode for parsing different sections

    // Initialize variables
    mode = 0;
    mdx_numxyz = 0;
    mdx_numtris = 0;
    mdx_numsfxobj = 0;
    mdx_numcommands = 0;
    memset(mdx_triangles, 0, sizeof(mdx_triangles));
    memset(mdx_xyz, 0, sizeof(mdx_xyz));
    memset(&model_mdx, 0, sizeof(model_mdx));
//    dword_E3F900 = -1082130432; // Initialize some floating-point value (-2.0f)//FREDZ nowhere else used?

    filename[0] = '\0'; // Initialize filename as an empty string

    // Process tokens from the script
    while (1)
    {
        GetToken(true);
        if (token[0] == '$' || endofscript)
            break;

        // Handle mode switches based on the token
        if (token[0] == '-')
        {
            if (strcmp(token, "-objects") == 0)
            {
                mode = 1;
            }
            else if (strcmp(token, "-sfx_define") == 0)
            {
                mode = 2;
            }
            else if (strcmp(token, "-sfx") == 0)
            {
                mode = 3;
            }
        }
        else if (mode)
        {
            // Handle different modes
            if (mode == 1)
            {
                printf("Adding %s.md2 as sub-object\n", token);
                LoadMD2File(token);
                ++mdx_numsfxobj;
            }
            // Modes 2 and 3 can be implemented similarly
        }
        else
        {
            // Set the MDX filename
            printf("Creating MDX: %s.mdx\n\n", token);
            strcpy(filename, token);
        }
    }

    // Ensure a filename was specified
    if (strlen(filename) < 1)
    {
        Error("Line %i has MDX without a filename specified\n\nFormat is:\n\n$mdx (mdx_name) -objects {md2 md2 ..}\n\n", scriptline);
    }

    // Write the MDX model file
    WriteMDXModelFile(filename);
    printf("\nCompleted successfully.\n\n");

    // Handle the end of the script token
    if (token[0] == '$')
        UnGetToken();
}

int mdxlenght;
byte *mdxfilename;
int currentDefineIndex;
int currentSpriteIndex;

void Cmd_Sfx_Load(void)
{
    int file_length; // Length of the file
    char filename[128]; // Buffer for the filename
    FILE *file; // File pointer
    size_t size_length; // Size of the file

    // Get the token and construct the filename
    GetToken(true);
    strcpy(filename, token);
    strcat(filename, ".mdx");

    // Open the file in binary read mode
    file = fopen(filename, "rb");
    if (!file)
    {
        fprintf(stderr, "reader: could not open file '%s'\n", filename);
        exit(0);
    }

    // Get the file length
    file_length = KP_filelength(file);
    mdxlenght = file_length;
    size_length = file_length;

    // Allocate memory for the file contents
    mdxfilename = (byte *)malloc(file_length + 100000);
    if (!mdxfilename)
    {
        fprintf(stderr, "reader: memory allocation failed\n");
        fclose(file);
        exit(0);
    }

    // Read the file into the allocated buffer
    fread(mdxfilename, 1, size_length, file);

    // Close the file
    fclose(file);

    // Initialize indices
    currentDefineIndex = 0;
    currentSpriteIndex = 0;

    // Print success message
    printf("\n--------------------------\nSuccessfully loaded %s\n\n", filename);
}

void Cmd_Sfx_Save(void)
{
    int i;
    int offset;
    dmdxl_t *mdx;
    char name[128];
    FILE *file;

    // Get the filename token
    GetToken(true);

    // Check if there is MDX data available
    if (!mdxlenght) {
        Error("Unable to save MDX on line %i, no MDX data available.\n\n", scriptline);
    }

    // Construct the MDX filename
    strcpy(name, token);
    strcat(name, ".mdx");

    // Open the file for writing
    file = fopen(name, "wb");
    if (!file) {
        fprintf(stderr, "writer: could not open file '%s'\n", name);
        exit(0);
    }

    // Cast the MDX filename buffer to the MDX structure
    mdx = (dmdxl_t *)mdxfilename;

    // Convert and write the MDX header
    for (i = 0; i < 23; ++i) {
        *(&mdx->ident + i) = LittleLong(*((int*)(mdxfilename + 4 * i)));
    }

    // Adjust the offset for sfx bounding box
    offset = mdx->ofs_sfxbbox;

    // Update MDX structure with current define and sprite indices
    mdx->num_sfxdef = currentDefineIndex;
    mdx->num_sfxent = currentSpriteIndex;
    mdx->ofs_sfxent = 68 * currentDefineIndex + mdx->ofs_sfxdef;
    mdx->ofs_sfxbbox = 76 * currentSpriteIndex + mdx->ofs_sfxent;
    mdx->ofs_end = 24 * mdx->num_frames * mdx->num_sfxobj + mdx->ofs_sfxbbox;

    // Write the MDX file
    fwrite(mdxfilename, 1, mdx->ofs_sfxdef, file);
    if (currentDefineIndex) {
        fwrite((char *)&model_mdx + 160, 1, 68 * currentDefineIndex, file);
    }
/*    if (currentSpriteIndex) {//FREDZ todo.
        fwrite(unk_E4C640, 1, 76 * currentSpriteIndex, file);
    }*/
    fwrite(&mdxfilename[offset], 1, mdxlenght - offset, file);


    // Close the file and free the MDX filename buffer
    fclose(file);
    free(mdxfilename);

    // Print success message
    printf("\nSuccessfully saved %s\n--------------------------\n\n", name);
}

typedef struct {
    int vertIndex;
    int defineIndex;
    int indexType;
    unsigned char frameArray[64];
} sfxAdd_t;

#define MAX_SFX_SPRITES 32

void Cmd_Sfx_Add(void)
{
    int startFrame, endFrame;
    sfxAdd_t *sfx;
    char *dashPos;
    int floorIndex;
    int frame;

    // Ensure we do not exceed the maximum number of SFX sprites
    if (currentSpriteIndex >= MAX_SFX_SPRITES) {
        Error("\nMAX_SFX_SPRITES (%i) exceeded on line %i, aborting.\n\n", MAX_SFX_SPRITES, scriptline);
    }

    // Allocate memory for a new SFX sprite
    sfx = (sizeof(sfxAdd_t) * currentSpriteIndex++);
    memset(sfx, 0, sizeof(sfxAdd_t)); // Clear the memory

    // Get the vertex index
    GetToken(true);
    sfx->vertIndex = atoi(token);

    // Get the SFX define index
    GetToken(true);
    sfx->defineIndex = atoi(token);

    // Ensure the SFX define index is valid
    if (!currentDefineIndex || sfx->defineIndex < 0 || sfx->defineIndex >= currentDefineIndex) {
        Error("Invalid SFX entry on line %i, sfx_define #%i doesn't exist yet.\n\n", scriptline, sfx->defineIndex);
    }

    // Get the index type (0 for vertex, 1 for triangle)
    GetToken(true);
    sfx->indexType = atoi(token);
    if (sfx->indexType != 0 && sfx->indexType != 1) {
        Error(
            "Invalid SFX index type on line %i, must use 0 (vertex) or 1 (triangle).\n\nSFX_Add syntax is:\n\n%s\n\n",
            scriptline,
            "$sfx_add <index_type, 0 (vertex) or 1 (triangle)> <index> <sfx_define_index> [(<frame_start> - <frame_end>) ...]\n"
            "\n"
            " eg: $sfx_add 0 50 1  10 to 18  40 to 50\n"
            "This creates an SFX sprite at vertex 50, using the first SFX define, in the given frame ranges"
        );
    }

    // Process frame ranges
    GetToken(true);
    while (token[0] != '$') {
        startFrame = atoi(token);
        dashPos = strstr(token, "-");

        if (dashPos) {
            endFrame = atoi(dashPos + 1);
        } else {
            GetToken(true);
            GetToken(true);
            endFrame = atoi(token);
        }

        for (frame = startFrame; frame <= endFrame; ++frame) {
            floorIndex = (int)floor((double)(frame / 8));
            sfx->frameArray[floorIndex] |= 128 >> (frame % 8);
        }

        GetToken(true);
    }

    if (token[0] == '$') {
        UnGetToken();
    }

    printf("Added sfx entry, using sfx_define #%i\n", sfx->defineIndex);
}

typedef struct {
    int type;
    int flags;
    int velocity_type;
    int velocity_speed_up;
    float gravity;
    int spawn_interval;
    float random_spawn_interval;
    float start_alpha;
    float end_alpha;
    float fadein_time;
    float lifetime;
    float random_time_scale;
    float start_width;
    float end_width;
    float start_height;
    float end_height;
    float random_size_scale;
} define_t;

typedef struct {
    define_t *sfxdefintions;
} sfxdefine_t;

#define MAX_SFX_DEFINE 16

void Cmd_Sfx_Define(void)
{
    sfxdefine_t sfx;

    // Ensure we do not exceed the maximum number of SFX defines
    if (currentDefineIndex >= MAX_SFX_DEFINE) {
        Error("\nMAX_SFX_DEFINE (%i) exceeded on line %i, aborting.\n\n", MAX_SFX_DEFINE, scriptline);
    }

    // Allocate memory for a new SFX definition
    sfx.sfxdefintions = (define_t *)((char *)&model_mdx + 68 * currentDefineIndex++ + 160);

    // Parse and set SFX definition fields
    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->type = atoi(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->flags = atoi(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->velocity_type = atoi(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->velocity_speed_up = atoi(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->gravity = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->spawn_interval = atoi(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->random_spawn_interval = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->start_alpha = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->end_alpha = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->fadein_time = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->lifetime = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->random_time_scale = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->start_width = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->end_width = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->start_height = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->end_height = atof(token);

    GetToken(true);
    if (token[0] == '$')
            Error("Invalid sfx_define at line %i, not enough arguments.\n\n", scriptline);
    sfx.sfxdefintions->random_size_scale = atof(token);

    // Print success message
    printf("Added sfx define #%i\n", currentDefineIndex - 1);
}


