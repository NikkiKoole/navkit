#!/usr/bin/env lua
--[[
  Generate Grids-style probability maps from drum-patterns.lua

  Usage: lua generate_prob_maps.lua <path-to-drum-patterns.lua> [--c | --csv | --preview]

  Reads ~645 real drum patterns, groups by genre, computes per-step hit frequency
  across all patterns in each genre, and outputs C RhythmProbMap structs.

  Instrument mapping to 4 tracks:
    kick:  BD
    snare: SD, RS, CPS (claps)
    hihat: CH, OH
    perc:  CY, CB, LT, MT, HT, TB

  Accent (AC) boosts velocity of co-occurring hits rather than being its own track.
]]

-- ============================================================================
-- CONFIGURATION
-- ============================================================================

-- How to map Lua genre categories to output style names
-- Multiple source categories can merge into one output style
local GENRE_MAP = {
    -- Output style name     = { list of source category name patterns }
    ["Rock"]                 = {"^Rock", "^Twist"},
    ["Pop"]                  = {"^Pop"},
    ["Disco"]                = {"^Disco"},
    ["Funk"]                 = {"^Funk"},
    ["Bossa Nova"]           = {"^Bossa Nova", "^Bossa$", "^BOSSANOVA"},
    ["Cha-Cha"]              = {"^Cha%-Cha", "^ChaCha"},
    ["Swing"]                = {"^Swing"},
    ["Reggae"]               = {"^Reggae", "^Dub"},
    ["Hip Hop"]              = {"^Hip Hop", "^BoomBAP", "^Miami Bass"},
    ["House"]                = {"^House"},
    ["Latin"]                = {"^Afro%-Cuban", "^Samba", "^Afro Cuban", "^Tango", "^Paso", "^Charleston"},
    ["Shuffle"]              = {"^Shuffle", "^Boogie"},
    ["Ballad"]               = {"^Ballad", "^Slow"},
    ["Blues"]                 = {"^Blues"},
    ["DnB"]                  = {"^Drum and Bass", "^Jungle"},
    ["Electro"]              = {"^Electro", "^EDM"},
    ["Ska"]                  = {"^Ska"},
    ["R&B"]                  = {"^Rhythm & Blues", "^RnB"},
    ["Basic"]                = {"^Basic Patterns"},
    ["Standard Breaks"]      = {"^Standard Breaks"},
    ["Waltz"]                = {"^Waltz"},
    ["March"]                = {"^March"},
    ["Jazz"]                 = {"^Jazz"},
    ["Endings"]              = {"^Ending"},
}

-- Instrument -> track mapping
local TRACK_MAP = {
    BD  = "kick",
    SD  = "snare",
    RS  = "snare",
    CPS = "snare",
    CH  = "hihat",
    OH  = "hihat",
    CY  = "perc",
    CB  = "perc",
    LT  = "perc",
    MT  = "perc",
    HT  = "perc",
    TB  = "perc",
}

-- Suggested swing and BPM per genre (hand-tuned defaults)
local GENRE_DEFAULTS = {
    ["Rock"]             = { swing = 0,  bpm = 120 },
    ["Pop"]              = { swing = 0,  bpm = 110 },
    ["Disco"]            = { swing = 0,  bpm = 120 },
    ["Funk"]             = { swing = 4,  bpm = 100 },
    ["Bossa Nova"]       = { swing = 3,  bpm = 130 },
    ["Cha-Cha"]          = { swing = 0,  bpm = 120 },
    ["Swing"]            = { swing = 8,  bpm = 140 },
    ["Reggae"]           = { swing = 2,  bpm = 80  },
    ["Hip Hop"]          = { swing = 6,  bpm = 90  },
    ["House"]            = { swing = 0,  bpm = 124 },
    ["Latin"]            = { swing = 0,  bpm = 100 },
    ["Shuffle"]          = { swing = 10, bpm = 120 },
    ["Ballad"]           = { swing = 0,  bpm = 70  },
    ["Blues"]             = { swing = 6,  bpm = 100 },
    ["DnB"]              = { swing = 0,  bpm = 170 },
    ["Electro"]          = { swing = 0,  bpm = 130 },
    ["Ska"]              = { swing = 0,  bpm = 160 },
    ["R&B"]              = { swing = 3,  bpm = 95  },
    ["Basic"]            = { swing = 0,  bpm = 120 },
    ["Standard Breaks"]  = { swing = 0,  bpm = 120 },
    ["Waltz"]            = { swing = 0,  bpm = 140 },
    ["March"]            = { swing = 0,  bpm = 120 },
    ["Jazz"]             = { swing = 8,  bpm = 130 },
}

-- ============================================================================
-- PARSE LUA PATTERNS
-- ============================================================================

local function loadPatterns(path)
    -- Load the Lua file - it returns a table called 'patterns'
    -- We need to execute it in a sandbox
    local chunk, err = loadfile(path)
    if not chunk then
        -- Try wrapping: the file defines 'local patterns = {...}' then 'return patterns'
        -- Let's just read and eval
        local f = io.open(path, "r")
        if not f then error("Cannot open: " .. path) end
        local src = f:read("*a")
        f:close()

        -- The file has 'local patterns = { ... }' at top level
        -- Wrap it to return the table
        local wrapped = src .. "\nreturn patterns"
        chunk, err = load(wrapped, path)
        if not chunk then error("Parse error: " .. err) end
    end
    return chunk()
end

-- Parse a grid string into a table of 16 booleans.
-- Handles 16-step (straight 16ths) and 12-step (triplet feel) patterns.
-- 12-step triplet patterns are mapped to 16 steps:
--   triplet positions 1..12 map to 16th positions via nearest-beat rounding
--   (triplet beat = 3 subdivs, 16th beat = 4 subdivs)
local TRIPLET_TO_16 = {1, 1, 2, 3, 4, 5, 5, 6, 7, 8, 9, 9, 10, 11, 12, 13, 13, 14, 15, 16}
-- More precisely: triplet step i (1-12) maps to 16th step round(i * 16/12)
local function tripletTo16Map()
    local map = {}
    for i = 1, 12 do
        map[i] = math.floor((i - 1) * 16 / 12) + 1
    end
    return map
end
local T2S = tripletTo16Map()  -- {1,2,3,4,5,7,8,9,10,11,12,14} approximately

local function parseGrid(str)
    local hits = {}
    for i = 1, 16 do hits[i] = false end
    local len = #str
    if len >= 16 then
        -- Standard 16-step
        for i = 1, 16 do
            local ch = str:sub(i, i)
            hits[i] = (ch == "x" or ch == "X")
        end
    elseif len >= 12 then
        -- 12-step triplet: map to 16
        for i = 1, 12 do
            local ch = str:sub(i, i)
            if ch == "x" or ch == "X" then
                hits[T2S[i]] = true
            end
        end
    end
    return hits
end

-- ============================================================================
-- AGGREGATE BY GENRE
-- ============================================================================

local function matchesGenre(categoryName, patterns)
    for _, pat in ipairs(patterns) do
        if categoryName:match(pat) then return true end
    end
    return false
end

-- Also try to match individual section names for CR78 etc
local function categorizeSection(categoryName, sectionName)
    -- Try section name first for specifics like "Waltz1", "BOSSANOVA A"
    local combined = categoryName .. "/" .. sectionName
    for genre, pats in pairs(GENRE_MAP) do
        if matchesGenre(sectionName, pats) then return genre end
    end
    -- Fall back to category name
    for genre, pats in pairs(GENRE_MAP) do
        if matchesGenre(categoryName, pats) then return genre end
    end
    return nil -- unmatched
end

local function aggregate(allPatterns)
    -- genre -> { kick = {count[1..16]}, snare = {...}, hihat = {...}, perc = {...}, accent = {...}, total = N }
    local genres = {}

    local unmatched = {}

    for _, category in ipairs(allPatterns) do
        local catName = category.name
        if category.sections then
            for _, section in ipairs(category.sections) do
                local genre = categorizeSection(catName, section.name or "")
                if genre then
                    if not genres[genre] then
                        genres[genre] = {
                            kick   = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            snare  = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            hihat  = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            perc   = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            accent = {0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0},
                            total  = 0,
                            names  = {},
                        }
                    end
                    local g = genres[genre]
                    g.total = g.total + 1
                    table.insert(g.names, (section.name or "?"))

                    if section.grid then
                        for inst, gridStr in pairs(section.grid) do
                            if #gridStr >= 12 then
                                local hits = parseGrid(gridStr)
                                local track = TRACK_MAP[inst]
                                if track and g[track] then
                                    for i = 1, 16 do
                                        if hits[i] then g[track][i] = g[track][i] + 1 end
                                    end
                                elseif inst == "AC" then
                                    local ah = parseGrid(gridStr)
                                    for i = 1, 16 do
                                        if ah[i] then g.accent[i] = g.accent[i] + 1 end
                                    end
                                end
                            end
                        end
                    end
                else
                    unmatched[catName] = true
                end
            end
        end
    end

    return genres, unmatched
end

-- ============================================================================
-- COMPUTE PROBABILITY MAPS
-- ============================================================================

local function computeProbMap(genreData)
    local n = genreData.total
    if n == 0 then return nil end

    local map = { kick = {}, snare = {}, hihat = {}, perc = {}, accent = {} }

    for _, track in ipairs({"kick", "snare", "hihat", "perc", "accent"}) do
        for i = 1, 16 do
            local freq = genreData[track][i] / n  -- 0.0 to 1.0
            -- Map frequency to probability 0-255
            -- Direct linear mapping: if 100% of patterns have this hit, prob = 255
            -- But we want to boost common hits and suppress rare ones slightly
            -- Using a curve: prob = freq^0.7 * 255 (makes common hits more prominent)
            local prob = math.floor(freq ^ 0.7 * 255 + 0.5)
            if prob > 255 then prob = 255 end
            if prob < 0 then prob = 0 end
            map[track][i] = prob
        end
    end

    return map
end

-- ============================================================================
-- OUTPUT: C CODE
-- ============================================================================

local function probArrayToC(arr)
    local parts = {}
    for i = 1, 16 do
        table.insert(parts, string.format("%3d", arr[i]))
    end
    return table.concat(parts, ",")
end

local function stepLabels()
    return "/*    1  .  .  .  2  .  .  .  3  .  .  .  4  .  .  .  */"
end

local function outputC(genres, genreOrder)
    local lines = {}
    local function emit(s) table.insert(lines, s) end

    emit("// Auto-generated by generate_prob_maps.lua from 260 Drum Machine Patterns")
    emit("// Source: https://ia800700.us.archive.org/2/items/260DrumMachinePatterns/")
    emit("//")
    emit("// Each value is 0-255: probability of a hit at that step.")
    emit("// Threshold with density knob: hit fires when prob > (1.0 - density) * 255")
    emit("")
    emit("#ifndef PIXELSYNTH_RHYTHM_PROB_MAPS_H")
    emit("#define PIXELSYNTH_RHYTHM_PROB_MAPS_H")
    emit("")
    emit("#include <stdint.h>")
    emit("")
    emit("typedef struct {")
    emit("    uint8_t kick[16];")
    emit("    uint8_t snare[16];")
    emit("    uint8_t hihat[16];")
    emit("    uint8_t perc[16];")
    emit("    uint8_t accent[16];")
    emit("    int length;")
    emit("    int swingAmount;")
    emit("    int recommendedBpm;")
    emit("} RhythmProbMap;")
    emit("")

    -- Enum
    emit("typedef enum {")
    for i, genre in ipairs(genreOrder) do
        local enumName = "PROB_" .. genre:upper():gsub("[%s%-&']", "_"):gsub("__+", "_")
        local comma = (i < #genreOrder) and "," or ""
        emit(string.format("    %s = %d%s", enumName, i - 1, comma))
    end
    emit("} ProbMapStyle;")
    emit("")
    emit("#define PROB_MAP_COUNT " .. #genreOrder)
    emit("")

    -- Style names
    emit("static const char* probMapNames[PROB_MAP_COUNT] = {")
    for _, genre in ipairs(genreOrder) do
        emit(string.format('    "%s",', genre))
    end
    emit("};")
    emit("")

    -- Maps
    emit("static const RhythmProbMap probMaps[PROB_MAP_COUNT] = {")
    for _, genre in ipairs(genreOrder) do
        local data = genres[genre]
        if data then
            local map = computeProbMap(data)
            local defaults = GENRE_DEFAULTS[genre] or { swing = 0, bpm = 120 }
            emit(string.format("    // %s (%d source patterns: %s)",
                genre, data.total,
                table.concat(data.names, ", "):sub(1, 80)))
            emit("    {")
            emit("        " .. stepLabels())
            emit(string.format("        .kick   = {%s},", probArrayToC(map.kick)))
            emit(string.format("        .snare  = {%s},", probArrayToC(map.snare)))
            emit(string.format("        .hihat  = {%s},", probArrayToC(map.hihat)))
            emit(string.format("        .perc   = {%s},", probArrayToC(map.perc)))
            emit(string.format("        .accent = {%s},", probArrayToC(map.accent)))
            emit(string.format("        .length = 16, .swingAmount = %d, .recommendedBpm = %d",
                defaults.swing, defaults.bpm))
            emit("    },")
        end
    end
    emit("};")
    emit("")
    emit("#endif // PIXELSYNTH_RHYTHM_PROB_MAPS_H")

    return table.concat(lines, "\n")
end

-- ============================================================================
-- OUTPUT: PREVIEW (ASCII visualization)
-- ============================================================================

local function outputPreview(genres, genreOrder)
    local lines = {}
    local function emit(s) table.insert(lines, s) end

    local densityLevels = {0.3, 0.5, 0.7, 0.9}
    local trackNames = {"kick", "snare", "hihat", "perc"}
    local trackChars = {kick = "K", snare = "S", hihat = "H", perc = "P"}

    for _, genre in ipairs(genreOrder) do
        local data = genres[genre]
        if data then
            local map = computeProbMap(data)
            emit(string.format("\n=== %s (%d patterns) ===", genre, data.total))
            emit("Step:  1 . . . 2 . . . 3 . . . 4 . . .")

            -- Show raw probabilities
            emit("\nRaw probabilities:")
            for _, track in ipairs(trackNames) do
                local row = {}
                for i = 1, 16 do
                    local p = map[track][i]
                    if p >= 200 then table.insert(row, "#")
                    elseif p >= 128 then table.insert(row, "o")
                    elseif p >= 64 then table.insert(row, ".")
                    else table.insert(row, " ")
                    end
                end
                emit(string.format("  %s: |%s|  %s", trackChars[track], table.concat(row, " "), track))
            end

            -- Show at different densities
            for _, density in ipairs(densityLevels) do
                local threshold = math.floor((1.0 - density) * 255)
                emit(string.format("\nDensity %.1f (threshold %d):", density, threshold))
                for _, track in ipairs(trackNames) do
                    local row = {}
                    for i = 1, 16 do
                        if map[track][i] > threshold then
                            table.insert(row, trackChars[track])
                        else
                            table.insert(row, ".")
                        end
                    end
                    emit(string.format("  %s: |%s|", trackChars[track], table.concat(row, " ")))
                end
            end
        end
    end

    return table.concat(lines, "\n")
end

-- ============================================================================
-- OUTPUT: CSV (for spreadsheet analysis)
-- ============================================================================

local function outputCSV(genres, genreOrder)
    local lines = {}
    local function emit(s) table.insert(lines, s) end

    emit("genre,track,s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,s12,s13,s14,s15,s16,patternCount")
    for _, genre in ipairs(genreOrder) do
        local data = genres[genre]
        if data then
            local map = computeProbMap(data)
            for _, track in ipairs({"kick", "snare", "hihat", "perc", "accent"}) do
                local vals = {}
                for i = 1, 16 do table.insert(vals, map[track][i]) end
                emit(string.format("%s,%s,%s,%d", genre, track, table.concat(vals, ","), data.total))
            end
        end
    end

    return table.concat(lines, "\n")
end

-- ============================================================================
-- MAIN
-- ============================================================================

local function main()
    local inputPath = arg[1]
    local mode = arg[2] or "--preview"

    if not inputPath then
        io.stderr:write("Usage: lua generate_prob_maps.lua <drum-patterns.lua> [--c | --csv | --preview]\n")
        os.exit(1)
    end

    local allPatterns = loadPatterns(inputPath)
    io.stderr:write(string.format("Loaded %d categories\n", #allPatterns))

    local genres, unmatched = aggregate(allPatterns)

    -- Report
    local totalPatterns = 0
    local genreOrder = {}
    for genre, data in pairs(genres) do
        table.insert(genreOrder, genre)
        totalPatterns = totalPatterns + data.total
    end
    table.sort(genreOrder)
    io.stderr:write(string.format("Matched %d patterns across %d genres\n", totalPatterns, #genreOrder))

    if next(unmatched) then
        io.stderr:write("Unmatched categories: ")
        local um = {}
        for k in pairs(unmatched) do table.insert(um, k) end
        io.stderr:write(table.concat(um, ", ") .. "\n")
    end

    for _, genre in ipairs(genreOrder) do
        io.stderr:write(string.format("  %-20s: %3d patterns\n", genre, genres[genre].total))
    end

    -- Output
    if mode == "--c" then
        print(outputC(genres, genreOrder))
    elseif mode == "--csv" then
        print(outputCSV(genres, genreOrder))
    else
        print(outputPreview(genres, genreOrder))
    end
end

main()
