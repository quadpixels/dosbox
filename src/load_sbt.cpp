// 2025-02-06
// 2025-02-07: thing_loadbin works minimally ..

#include <assert.h>
#include <stdio.h>

#include <filesystem>

#define xstr(a) str(a)
#define str(a) #a

#ifndef _MSC_VER
#define fread_s(buf, bufsize, readsize, items, handle) fread(buf, readsize, items, handle)
#endif

int load_sound_sample_table(FILE* f) {
  printf(">> load_sound_sample_table\n");
  int iVar3{};
  char DAT_004cebd8[0x4b0]{};  // 1200
  char DAT_004c9c90[0xa8c]{};  // 2700
  int uVar4, iVar4, iVar5;
  char* puVar10{};

  iVar3 = fread_s(DAT_004cebd8, 0x4b0, 0x4b0, 1, f);
  if (iVar3 == 1) {
    iVar3 = fread_s(DAT_004c9c90, 0xa8c, 0xa8c, 1, f);
    if (iVar3 == 1) {
      puVar10 = DAT_004c9c90;
      iVar3 = 0;
      do {
        if ((0 < *(int*)(DAT_004cebd8 + iVar3)) && (*(char*)puVar10 != '\0')) {
#if 0
          uVar4 = get_sample_sub_entry(puVar10);
          iVar5 = strcmp(puVar10, uVar4);
          if (iVar5 == 0) {
            uVar4 = load_from_sounds_fldr(puVar10);
            *(int*)((int)&DAT_004ca720 + iVar3) = uVar4;
          }
#else
          printf("Loading sample: %s\n", puVar10);
#endif
        }
        puVar10 = puVar10 + 9;
        iVar3 = iVar3 + 4;
      } while (puVar10 < DAT_004c9c90 + 0xa8c);
    }
    puVar10 = DAT_004c9c90;
    iVar3 = 0;
    do {
      if ((0 < *(int*)(DAT_004cebd8 + iVar3)) && (*(char*)puVar10 != '\0')) {
#if 0
        pcVar6 = (char*)get_sample_sub_entry(puVar10);
        uVar7 = 0xffffffff;
        do {
          pcVar11 = pcVar6;
          if (uVar7 == 0) break;
          uVar7 = uVar7 - 1;
          pcVar11 = pcVar6 + 1;
          cVar1 = *pcVar6;
          pcVar6 = pcVar11;
        } while (cVar1 != '\0');
        uVar7 = ~uVar7;
        puVar9 = (undefined4*)(pcVar11 + -uVar7);
        puVar12 = local_10;
        for (uVar8 = uVar7 >> 2; uVar8 != 0; uVar8 = uVar8 - 1) {
          *puVar12 = *puVar9;
          puVar9 = puVar9 + 1;
          puVar12 = puVar12 + 1;
        }
        for (uVar7 = uVar7 & 3; uVar7 != 0; uVar7 = uVar7 - 1) {
          *(undefined*)puVar12 = *(undefined*)puVar9;
          puVar9 = (undefined4*)((int)puVar9 + 1);
          puVar12 = (undefined4*)((int)puVar12 + 1);
        }
        iVar5 = strcmp(local_10, puVar10);
        if (iVar5 != 0) {
          iVar5 = get_sample_name(local_10);
          if (iVar5 < 0) {
            *(undefined4*)((int)&DAT_004ca720 + iVar3) = 0xffffffff;
          }
          else {
            iVar2 = (&DAT_004cebd8)[iVar5];
            *(undefined4*)((int)&DAT_004ca720 + iVar3) = (&DAT_004ca720)[iVar5];
            (&DAT_004cebd8)[iVar5] = iVar2 + -1;
            (&DAT_004cf1a8)[iVar5] = (&DAT_004cf1a8)[iVar5] + 1;
          }
        }
#else
#endif
      }
      puVar10 = puVar10 + 9;
      iVar3 = iVar3 + 4;
      if (DAT_004c9c90 + 0xa8b < puVar10) {
        return 0;
      }
    } while (true);
  }
  else {
    printf("Error loading sound sample table\n");
  }
  return -1;
}

int read_sob_table(FILE* f) {
  int sob_table_count{};
  int iVar1, iVar2;
  char* puVar3, * pcVar4;
  char sob_table[0x14 * 200];

  printf(">> read_sob_table\n");
  iVar1 = fread_s(&sob_table_count, 4, 4, 1, f);
  printf("sob_table_count = %d\n", sob_table_count);
  
  if (0 < sob_table_count) {
    iVar1 = fread_s(sob_table, sizeof(sob_table), sob_table_count * 0x14, 1, f);
    iVar1 = 0;
    if (sob_table_count > 0) {
      puVar3 = sob_table;
      do {
#if 0
        iVar2 = load_sob_anim(puVar3);
        *(int*)(puVar3 + 0x10) = iVar2;
        if (iVar2 == 0) {
          log_message(s_Unable_to_load_SOB_or_SOO__s__004bb29c, puVar3);
        }
#else
        printf("Load SOB anim for %s\n", puVar3);
#endif
        iVar1 = iVar1 + 1;
        puVar3 = puVar3 + 0x14;
      } while (iVar1 < sob_table_count);
    }
  }

  return 0;
}

int polyhandle_loadbin(FILE* f) {
  printf(">> polyhandle_loadbin\n");
  int num_poly_handles;
  char select_poly_handle[500 * 4];
  char poly_handle[500 * 4];
  char poly_handle_texture[500 * 4];
  char script_ref_poly[500 * 4];

  fread_s(&num_poly_handles, 4, 4, 1, f);
  printf("num_poly_handles = %d\n", num_poly_handles);

  assert(1 == fread_s(select_poly_handle, sizeof(select_poly_handle), num_poly_handles * 4, 1, f));
  assert(1 == fread_s(poly_handle, sizeof(poly_handle), num_poly_handles * 4, 1, f));
  assert(1 == fread_s(poly_handle_texture, sizeof(poly_handle_texture), num_poly_handles * 4, 1, f));
  assert(1 == fread_s(script_ref_poly, sizeof(script_ref_poly), num_poly_handles * 4, 1, f));

  for (int i = 0; i < num_poly_handles; i++) {
    int s = ((int*)(select_poly_handle))[i];
    int ph = ((int*)(select_poly_handle))[i];
    int pht = ((int*)(select_poly_handle))[i];
    int srp = ((int*)(select_poly_handle))[i];
    printf("  [%d]: select=0x%08x, h=0x%08x, ht=0x%08x, srp=0x%08x\n", i, s, ph, pht, srp);
  }
  
  return 0;
}

int objecthandle_loadbin(FILE* f) {
  printf(">> objecthandle_loadbin\n");
  int num_obj_handles;
  char DAT_00617658[300 * 4];
  char obj_handle[300 * 4];
  char script_obj_handle[300 * 4];

  fread_s(&num_obj_handles, 4, 4, 1, f);
  printf("num_obj_handles = %d\n", num_obj_handles);

  assert(1 == fread_s(DAT_00617658, sizeof(DAT_00617658), num_obj_handles * 4, 1, f));
  assert(1 == fread_s(obj_handle, sizeof(obj_handle), num_obj_handles * 4, 1, f));
  assert(1 == fread_s(script_obj_handle, sizeof(script_obj_handle), num_obj_handles * 4, 1, f));

  for (int i = 0; i < num_obj_handles; i++) {
    int d = ((int*)(DAT_00617658))[i];
    int oh = ((int*)(obj_handle))[i];
    int soh = ((int*)(script_obj_handle))[i];
    printf("  [%d]: d=0x%08x, oh=0x%08x, soh=0x%08x\n", i, d, oh, soh);
  }

  return 0;
}

int sectorhandle_loadbin(FILE* f) {
  printf(">> sectorhandle_loadbin\n");
  int num_sec_handles;
  char DAT_005b1df8[256 * 4];
  char script_sec_num[256 * 4];

  fread_s(&num_sec_handles, 4, 4, 1, f);
  printf("num_sec_handles = %d\n", num_sec_handles);

  assert(1 == fread_s(DAT_005b1df8, sizeof(DAT_005b1df8), num_sec_handles * 4, 1, f));
  assert(1 == fread_s(script_sec_num, sizeof(script_sec_num), num_sec_handles * 4, 1, f));

  for (int i = 0; i < num_sec_handles; i++) {
    int d = ((int*)(DAT_005b1df8))[i];
    int ssn = ((int*)(script_sec_num))[i];
    printf("  [%d]: d=0x%08x, ssn=0x%08x\n", i, d, ssn);
  }

  return 0;
}

int switch_loadbin(FILE* f) {
  printf(">> switch_loadbin\n");
  int num_switches;
  char switches[512 * 32];
  assert(1 == fread_s(&num_switches, 4, 4, 1, f));
  printf("  num_switches = %d\n", num_switches);
  assert(1 == fread_s(switches, 512 * 32, num_switches * 32, 1, f));
  return 0;
}

int door_loadbin(FILE* f) {
  printf(">> door_loadbin\n");
  int num_doors;
  char doors[100 * 0xb8];
  assert(1 == fread_s(&num_doors, 4, 4, 1, f));
  printf("  num_doors=%d\n", num_doors);
  assert(1 == fread_s(doors, 100 * 0xb8, num_doors * 0xb8, 1, f));
  return 0;
}

// Looks like only appears when loading an existing save game
int thing_loadbin(FILE* f) {
  printf(">> thing_loadbin\n");
  const int N = 260;
  char thing_pointer[0x528 * N];  // 7ffd20 - 7ac080, size=0x53CA0. 260 objects
  int local_24{};
  char local_31{};
  char thing_buf[0x528];  // Descriptor?
  char* puVar6 = thing_pointer;

  do {
    // 0x0025dff1      mov ebx, 1
    // 0x0025dff6      lea eax, [var_30h]
    // 0x0025dffa      mov ecx, esi
    // 0x0025dffc      mov edx, ebx   // 1
    // 0x0025dffe      call fread
    char tmp{};
    assert(1 == fread_s(&tmp, 1, 1, 1, f));
    if (tmp != 0) {
      assert(1 == fread_s(thing_buf, 0x528, 0x528, 1, f));
      // TODO: figure out what happens here
    }

    puVar6 += 0x528;
    if (puVar6 >= thing_pointer + 0x53CA0) {
      return 0;
    }
    local_24 += 1;
  } while (true);

  return 0;
}

int thingtypes_loadbin(FILE* f) {
  printf(">> thingtypes_loadbin\n");
  int num_thing_types;
  char thing_types[128 * 0x3a8];
  assert(1 == fread_s(&num_thing_types, 4, 4, 1, f));
  printf("num_thing_types = %d\n", num_thing_types);
  assert(1 == fread_s(&thing_types, sizeof(thing_types), num_thing_types * 0x3a8, 1, f));
  for (int i = 0; i < num_thing_types; i++) {
    printf("  [%d]: %s\n", i, thing_types + i * 0x3a8);
  }
  return 0;
}

int nameditems_loadbin(FILE* f) {
  printf(">> nameditems_loadbin\n");
  int num_named_items;
  char named_items[0x34 * 50];
  assert(1 == fread_s(&num_named_items, 4, 4, 1, f));
  printf("num_named_items = %d\n", num_named_items);

  assert(1 == fread_s(named_items, sizeof(named_items), num_named_items * 0x34, 1, f));
  for (int i = 0; i < num_named_items; i++) {
    printf("  [%d]: %s\n", i, named_items + i * 0x34);
  }

  return 0;
}

int sprite_loadbin(FILE* f) {
  printf(">> sprite_loadbin\n");
  int num_sprites;
  char sprites[500 * 0x2c];
  assert(1 == fread_s(&num_sprites, 4, 4, 1, f));
  assert(1 == fread_s(sprites, 500 * 0x2c, num_sprites * 0x2c, 1, f));
  return 0;
}

int soundsources_loadbin(FILE* f) {
  printf(">> soundsources_loadbin\n");
  int num_snd_sources{};
  assert(1 == fread_s(&num_snd_sources, 4, 4, 1, f));
  printf("  num_snd_sources = %d\n", num_snd_sources);
  char snd_source_x[127 * 4];
  char snd_source_y[127 * 4];
  char snd_source_z[127 * 4];
  char snd_source_vol[127 * 4];
  char snd_source_sample_id[127 * 4];
  char snd_source_decay[127 * 8];
  assert(1 == fread_s(&snd_source_x, 127 * 4, num_snd_sources * 4, 1, f));
  assert(1 == fread_s(&snd_source_y, 127 * 4, num_snd_sources * 4, 1, f));
  assert(1 == fread_s(&snd_source_z, 127 * 4, num_snd_sources * 4, 1, f));
  assert(1 == fread_s(&snd_source_vol, 127 * 4, num_snd_sources * 4, 1, f));
  assert(1 == fread_s(&snd_source_sample_id, 127 * 4, num_snd_sources * 4, 1, f));
  assert(1 == fread_s(&snd_source_decay, 127 * 8, num_snd_sources * 8, 1, f));
  return 0;
}

int sectorstuff_loadbin(FILE* f) {
  printf(">> sectorstuff_loadbin\n");
  char sector_mvmt_x[0x5800];
  assert(1 == fread_s(sector_mvmt_x, 0x5800, 0x5800, 1, f));
  return 0;
}

int playerstatus_loadbin() {
  assert(0 && "unimplemented");
  return 0;
}

int lochandle_loadbin(FILE* f) {
  int num_locations{};
  int num_scripts{};
  char thing_loc_handle[200 * 0x28];
  char script_event_id[1024 * 4];
  char compiled_scripts[1024 * 4];
  char DAT_00616658[1024 * 4];
  int compiled_scripts_len{};
  char selected_poly[0x30000];  // 196608
  char DAT_005e4278[1024];
  char num_sleeping_scripts[1024];
  char DAT_00616258[1024];
  char DAT_005e5e58[1024];
  char DAT_005e2dd8[1024];
  char DAT_005e39d8[1024];
  char DAT_005e35d8[1024];
  char DAT_005e31d8[1024];
  char DAT_005b29c8[1024];

  printf(">> lochandle_loadbin\n");
  assert(1 == fread_s(&num_locations, 4, 4, 1, f));
  assert(1 == fread_s(thing_loc_handle, 200 * 0x28, num_locations * 0x28, 1, f));
  assert(1 == fread_s(&num_scripts, 4, 4, 1, f));
  assert(1 == fread_s(script_event_id, 1024 * 4, num_scripts * 4, 1, f));
  assert(1 == fread_s(compiled_scripts, 1024 * 4, num_scripts * 4, 1, f));
  assert(1 == fread_s(DAT_00616658, 1024 * 4, num_scripts * 4, 1, f));
  assert(1 == fread_s(&compiled_scripts_len, 4, 4, 1, f));
  assert(1 == fread_s(selected_poly, 0x30000, compiled_scripts_len, 1, f));
  printf("num_locations = %d, num_scripts = %d, compiled_scripts_len = %d\n",
    num_locations, num_scripts, compiled_scripts_len);
  int param_1;
  assert(1 == fread_s(&param_1, 4, 1, 1, f));
  if ((param_1 & 0xFF) == 0) {
    return 0;
  }
  assert(1 == fread_s(DAT_005e4278, 1024, 1024, 1, f));
  assert(1 == fread_s(num_sleeping_scripts, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_00616258, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005e5e58, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005e2dd8, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005e39d8, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005e35d8, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005e31d8, 1024, 1024, 1, f));
  assert(1 == fread_s(DAT_005b29c8, 1024, 1024, 1, f));
  return 0;
}

int lifts_loadbin(FILE* f) {
  assert(0 && "unimplemented");
  return 0;
}

int read_item_info(FILE* f) {
  printf(">> read_item_info\n");
  // if (game_hostmode != 1)
  char item_pitch[0x7080];
  assert(1 == fread_s(item_pitch, 0x7080, 0x7080, 1, f));
  char* ptr = item_pitch;
  int num_items_singleplayer = 0;
  int num_items_dm = 0;
  do {
    int iVar2 = *((int*)ptr);
    printf("%x ", iVar2);
    if (iVar2 == 0 || iVar2 == -4 || iVar2 == -3) {
      num_items_singleplayer++;
    }
    else {
      if (iVar2 >= 1) {
        num_items_dm++;
      }
    }
    ptr += 0x18 * 4;
  } while (ptr < item_pitch + 0x7080);
  printf("\n");
  printf("  %d singleplayer items, %d dm items\n",
    num_items_singleplayer, num_items_dm);
  return 0;
}

int main() {
  const char* fn = "SAVED8.SBT";
  FILE* f{};
  #ifndef _MSC_VER
  f = fopen(fn, "rb");
  #else
  fopen_s(&f, fn, "rb");
  #endif
  fseek(f, 0, SEEK_END);
  long s = ftell(f);
  fseek(f, 0, SEEK_SET);
  printf("file %s has %ld = 0x%lx bytes.\n", fn, s, s);

  int param_2 = 1;
  char auStack_50[80];
  char auStack_b4[80];
  int iStack_b8{};

  // Read SBT header

  // Arg binding for fread is:
  // fread(buffer, EDX /*size*/, EBX /*count*/, f)

  // 0x0026e86b      mov ebx, 1
  // 0x0026e870      mov edx, 0x50; 'P'
  // 0x0026e875      lea eax, [var_64h]
  // 0x0026e879      mov ecx, esi
  // 0x0026e87b      call fread; _fread
  assert(1 == fread_s(auStack_50, sizeof(auStack_50), 0x50, 1, f));

  // 0x0026e889      mov edx, 4
  // 0x0026e88e      mov ecx, esi
  // 0x0026e890      mov ebx, eax
  // 0x0026e892      lea eax, [var_b4h]
  // 0x0026e899      call fread; _fread
  assert(1 == fread_s(&iStack_b8, sizeof(iStack_b8), 4, 1, f));
  assert(iStack_b8 == 0);  // SBT version must be 0

  // 0x0026e996      mov edx, 0x50; 'P'
  // 0x0026e99b      mov ecx, esi
  // 0x0026e99d      mov ebx, eax
  // 0x0026e99f      mov eax, 0x48fe0c
  // 0x0026e9a4      call fread; _fread
  assert(1 == fread_s(auStack_50, sizeof(auStack_50), 0x50, 1, f));  // header?

#define DECLARE_AND_READ_INT(x) \
  int x; assert(1==fread_s(&x, 4, 4, 1, f)); \
  printf("" xstr(x) " = %d\n", x);

  DECLARE_AND_READ_INT(DAT_004aa38c);
  DECLARE_AND_READ_INT(gm_mem_loc);
  DECLARE_AND_READ_INT(next_level_num);
  DECLARE_AND_READ_INT(num_kills);
  DECLARE_AND_READ_INT(dm_game_type);
  DECLARE_AND_READ_INT(curr_level);
  DECLARE_AND_READ_INT(mp_game_flags);
  DECLARE_AND_READ_INT(DAT_0083d188);
  DECLARE_AND_READ_INT(DAT_0083d130);
  DECLARE_AND_READ_INT(DAT_0083d198);
  DECLARE_AND_READ_INT(game_hostmode);
  DECLARE_AND_READ_INT(DAT_0083d19c);
  DECLARE_AND_READ_INT(DAT_0083d18c);
  DECLARE_AND_READ_INT(DAT_0083d194);
  DECLARE_AND_READ_INT(side_screen);
  DECLARE_AND_READ_INT(DAT_0083d300);
  DECLARE_AND_READ_INT(DAT_0083d180);
  DECLARE_AND_READ_INT(DAT_0083d63c);
  DECLARE_AND_READ_INT(DAT_0083d62c);
  DECLARE_AND_READ_INT(DAT_0083d2d8);
  DECLARE_AND_READ_INT(DAT_0083d638);
  DECLARE_AND_READ_INT(DAT_0083d2c0);

  char DAT_0083d2e0[32];
  assert(1 == fread_s(DAT_0083d2e0, 32, 32, 1, f));

  char s_none_004aa2d8[32];
  assert(1 == fread_s(s_none_004aa2d8, 32, 32, 1, f));

  printf(">> Start\n");

  int uStack_bc{}, keyword{};
  int sbt_data = fread_s(&uStack_bc, 4, 1, 1, f);
  do {
    if (sbt_data != 1) {
      printf("Error reading SBT header.\n");
      assert(0);
    }
    printf("uStack_bc = 0x%x, offset = 0x%lx\n", (uStack_bc & 0xFF), ftell(f));
    switch (uStack_bc & 0xFF) {
    case 1:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Switch_encountered_on_things_loa_004acc24, file_name);");
        return 0xfffffff2;
      }
      sbt_data = switch_loadbin(f);
      break;
    case 2:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Door_encountered_on_things_load___004acbec, file_name);");
        return 0xfffffff2;
      }
      sbt_data = door_loadbin(f);
      break;
    case 3:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Thing_encountered_on_things_load_004acbb0, file_name);");
        return 0xfffffff2;
      }
      sbt_data = thing_loadbin(f);
      break;
    case 4:
      sbt_data = thingtypes_loadbin(f);
      break;
    case 5:
      sbt_data = nameditems_loadbin(f);
      break;
    case 6:
      sbt_data = sprite_loadbin(f);
      if (sbt_data != 0) {
        return 0xfffffff7;
      }
      goto LAB_0045535c;
    case 7:
      sbt_data = soundsources_loadbin(f);
      if (sbt_data != 0) {
        return 0xfffffff6;
      }
      goto LAB_0045535c;
    case 8:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Sector_stuff_encountered_on_thin_004acb34, file_name);");
        return 0xfffffff2;
      }
      sbt_data = sectorstuff_loadbin(f);
      break;
    case 9:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Player_Status_encountered_on_thi_004acc60, file_name);");
        return 0xfffffff2;
      }
      sbt_data = playerstatus_loadbin();
      break;
    case 10:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Other_encountered_on_things_load_004acb74, file_name);");
        return 0xfffffff2;
      }
      sbt_data = -1;
      break;
    case 0xb:
      sbt_data = lochandle_loadbin(f);
      if (sbt_data != 0) {
        return 0xfffffff4;
      }
      goto LAB_0045535c;
    case 0xc: {
      sbt_data = load_sound_sample_table(f);
      goto joined_r0x0045524e;
      assert(0 && "Unimplemented");
    }
    case 0xf: {
      sbt_data = polyhandle_loadbin(f);
      goto joined_r0x004551e0;
    }
    case 0x10: {
      sbt_data = objecthandle_loadbin(f);
      goto joined_r0x004551e0;
    }
    case 0x11:
      sbt_data = sectorhandle_loadbin(f);
    joined_r0x004551e0:
      if (sbt_data != 0) {
        return -1;
      }
      goto LAB_0045535c;
    case 0x12:
      sbt_data = lifts_loadbin(f);
      goto joined_r0x00455356;
    case 0x13:
      if (param_2 == 0) {
        assert(0 && "log_message(s_Item_encountered_on_things_load___004acafc, file_name);");
        return 0xfffffff2;
      }
      sbt_data = read_item_info(f);
    joined_r0x00455356:
      if (sbt_data != 0) {
        return 0xfffffffb;
      }
      goto LAB_0045535c;
    case 0x14: {
      sbt_data = read_sob_table(f);
    joined_r0x0045524e:
      if (sbt_data != 0) {
        return -1;
      }
      goto LAB_0045535c;
    }
    default: {
      printf("case is 0x%x, not implemented.\n", uStack_bc & 0xFF);
      abort();
      exit(0);
      break;
    }
    }
  LAB_0045535c:
    keyword = uStack_bc & 0xFF;
    sbt_data = fread_s(&uStack_bc, 4, 1, 1, f);
  } while (true);

  return 0;
}