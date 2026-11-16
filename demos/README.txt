DEMO SONGS (MobileDAW)
======================
meadow-wind.mdaw  "Meadow Wind" - orchestral, 80 BPM, D major, 2:48. Original composition (gentle film-orchestra style).
                  Instruments: SoundFont (piano, harp, flute, oboe, clarinet, strings, cello, bass, pizzicato, horns, brass,
                  timpani, choir, celesta, glockenspiel, orchestra kit) + Osphere (Cinematic Swell, Air Choir).
ghibli-sky.mdaw   "Ghibli Sky" - piano + professional orchestra, 84 BPM, F major, 3:03. Original composition in the style of
                  animated-film theme music (no melody is taken from any existing film or song).
                  Form: 8-bar piano intro, theme A on piano, theme A on flute, theme B on oboe, theme B on strings, full-orchestra
                  climax (strings, flute, horns, brass, timpani, choir, glockenspiel), quiet piano reprise, outro.
                  Instruments: same SoundFont set as Meadow Wind + Osphere (Swell, Air) with a volume automation lane.

progressive-house.mdaw  "Neon Horizon" - progressive house, 128 BPM, F minor, 2:00 (64 bars). Original composition, the showcase of the
                  interactive tutorial (Menu -> Tutorial interaktif -> Showcase). Built by tools/make-proghouse.mjs with the project API.
                  Form: Intro 1-8, Build 9-16, Groove 17-24, Breakdown 25-32 (riser + snare roll), Drop 33-48, Drop B 49-56, Outro 57-64.
                  Instruments: drum sampler (synthesized kit), MiniSynth (rolling bass), GMS (pluck arp), SuperSaw (pad, lead, riser);
                  Autoduck sidechain from the kick, Auto Filter / riser pitch / master volume automation, Reverb + Delay returns,
                  Glue Compressor + Limiter on the master. No samples or SoundFont needed.

Files used
----------
SoundFonts/TimGM6mb.sf2   (c) 2004 Tim Brechbill, 2010 David Bolton. Licence: GNU GPL v2 (see https://www.gnu.org/licenses/old-licenses/gpl-2.0.html).
                  It is a third-party file: if you publish the app publicly, either keep the GPL notice with it or replace it by a
                  SoundFont whose licence you are happy with (the projects only refer to the path /SoundFonts/TimGM6mb.sf2).

The first time a demo is opened, the app copies the SoundFont into its library (/SoundFonts).
