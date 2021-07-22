#include "music.h"

// notes of the moledy followed by the duration.
// a 4 means a quarter note, 8 an eighteenth , 16 sixteenth, so on
// !!negative numbers are used to represent dotted notes,
// so -4 means a dotted quarter note, that is, a quarter plus an eighteenth!!

const int melody_dingdong[] = {
  NOTE_B4, 8, NOTE_G4, 4
};

const int melody_pacman[] = {
  // Pacman
  // Score available at https://musescore.com/user/85429/scores/107109
  NOTE_B4, 16, NOTE_B5, 16, NOTE_FS5, 16, NOTE_DS5, 16, //1
  NOTE_B5, 32, NOTE_FS5, -16, NOTE_DS5, 8, NOTE_C5, 16,
  NOTE_C6, 16, NOTE_G6, 16, NOTE_E6, 16, NOTE_C6, 32, NOTE_G6, -16, NOTE_E6, 8,

  NOTE_B4, 16,  NOTE_B5, 16,  NOTE_FS5, 16,   NOTE_DS5, 16,  NOTE_B5, 32,  //2
  NOTE_FS5, -16, NOTE_DS5, 8,  NOTE_DS5, 32, NOTE_E5, 32,  NOTE_F5, 32,
  NOTE_F5, 32,  NOTE_FS5, 32,  NOTE_G5, 32,  NOTE_G5, 32, NOTE_GS5, 32,  NOTE_A5, 16, NOTE_B5, 8
};

const int melody_PinkPanther[] = {

  // Pink Panther theme
  // Score available at https://musescore.com/benedictsong/the-pink-panther
  // Theme by Masato Nakamura, arranged by Teddy Mason

  REST,2, REST,4, REST,8, NOTE_DS4,8, 
  NOTE_E4,-4, REST,8, NOTE_FS4,8, NOTE_G4,-4, REST,8, NOTE_DS4,8,
  NOTE_E4,-8, NOTE_FS4,8,  NOTE_G4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_E4,8, NOTE_G4,-8, NOTE_B4,8,   
  NOTE_AS4,2, NOTE_A4,-16, NOTE_G4,-16, NOTE_E4,-16, NOTE_D4,-16, 
  NOTE_E4,2, REST,4, REST,8, NOTE_DS4,4,

  NOTE_E4,-4, REST,8, NOTE_FS4,8, NOTE_G4,-4, REST,8, NOTE_DS4,8,
  NOTE_E4,-8, NOTE_FS4,8,  NOTE_G4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_G4,8, NOTE_B4,-8, NOTE_E5,8,
  NOTE_DS5,1,   
  NOTE_D5,2, REST,4, REST,8, NOTE_DS4,8, 
  NOTE_E4,-4, REST,8, NOTE_FS4,8, NOTE_G4,-4, REST,8, NOTE_DS4,8,
  NOTE_E4,-8, NOTE_FS4,8,  NOTE_G4,-8, NOTE_C5,8, NOTE_B4,-8, NOTE_E4,8, NOTE_G4,-8, NOTE_B4,8,   
  
  NOTE_AS4,2, NOTE_A4,-16, NOTE_G4,-16, NOTE_E4,-16, NOTE_D4,-16, 
  NOTE_E4,-4, REST,4,
  REST,4, NOTE_E5,-8, NOTE_D5,8, NOTE_B4,-8, NOTE_A4,8, NOTE_G4,-8, NOTE_E4,-8,
  NOTE_AS4,16, NOTE_A4,-8, NOTE_AS4,16, NOTE_A4,-8, NOTE_AS4,16, NOTE_A4,-8, NOTE_AS4,16, NOTE_A4,-8,   
  NOTE_G4,-16, NOTE_E4,-16, NOTE_D4,-16, NOTE_E4,16, NOTE_E4,16, NOTE_E4,2,
 
};
const int melody_keyboardCat[] = {

  // Keyboard cat
  // Score available at https://musescore.com/user/142788/scores/147371

    NOTE_C4,4, NOTE_E4,4, NOTE_G4,4, NOTE_E4,4, 
    NOTE_C4,4, NOTE_E4,8, NOTE_G4,-4, NOTE_E4,4,
    NOTE_A3,4, NOTE_C4,4, NOTE_E4,4, NOTE_C4,4,
    NOTE_A3,4, NOTE_C4,8, NOTE_E4,-4, NOTE_C4,4,
    NOTE_G3,4, NOTE_B3,4, NOTE_D4,4, NOTE_B3,4,
    NOTE_G3,4, NOTE_B3,8, NOTE_D4,-4, NOTE_B3,4,

    NOTE_G3,4, NOTE_G3,8, NOTE_G3,-4, NOTE_G3,8, NOTE_G3,4, 
    NOTE_G3,4, NOTE_G3,4, NOTE_G3,8, NOTE_G3,4,
    NOTE_C4,4, NOTE_E4,4, NOTE_G4,4, NOTE_E4,4, 
    NOTE_C4,4, NOTE_E4,8, NOTE_G4,-4, NOTE_E4,4,
    NOTE_A3,4, NOTE_C4,4, NOTE_E4,4, NOTE_C4,4,
    NOTE_A3,4, NOTE_C4,8, NOTE_E4,-4, NOTE_C4,4,
    NOTE_G3,4, NOTE_B3,4, NOTE_D4,4, NOTE_B3,4,
    NOTE_G3,4, NOTE_B3,8, NOTE_D4,-4, NOTE_B3,4,

    NOTE_G3,-1, 
};

const int melody_Nokia[] = {

  // Nokia Ringtone 
  // Score available at https://musescore.com/user/29944637/scores/5266155
  
  NOTE_E5, 8, NOTE_D5, 8, NOTE_FS4, 4, NOTE_GS4, 4, 
  NOTE_CS5, 8, NOTE_B4, 8, NOTE_D4, 4, NOTE_E4, 4, 
  NOTE_B4, 8, NOTE_A4, 8, NOTE_CS4, 4, NOTE_E4, 4,
  NOTE_A4, 2, 
};

bool Music::add(uint16_t note, uint16_t delay)
{
  if(m_bPlaying)
  {
    if(m_idx >= MUS_LEN)
      return false;
    m_arr[m_idx].note = note;
    m_arr[m_idx].ms = delay;
    m_idx++;
  }
  else
  {
    playNote(note, delay);
    m_bPlaying = true;
  }
  return true;
}

void Music::playNote(int note, int duration)
{
    if(note)
    {
      analogWriteFreq(note);
      analogWrite(TONE, 40);
      m_volume = 39;
    }
    else
    {
      analogWrite(TONE, 0);      
      m_volume = 0;
    } 
    m_toneEnd = millis() + duration;
}

bool Music::play(int song)
{
  int tempo = 100;
  int notes;
  const int *pSong;

  switch(song)
  {
    case 0:
      tempo = 60;
      notes = sizeof(melody_dingdong) / sizeof(melody_dingdong[0]) / 2;
      pSong = melody_dingdong;
      break;
    case 1:
      tempo = 105;
      notes = sizeof(melody_pacman) / sizeof(melody_pacman[0]) / 2;
      pSong = melody_pacman;
      break;
    case 2:
      tempo = 120;
      notes = sizeof(melody_PinkPanther) / sizeof(melody_PinkPanther[0]) / 2;
      pSong = melody_PinkPanther;
      break;
    case 3:
      tempo = 160;
      notes = sizeof(melody_keyboardCat) / sizeof(melody_keyboardCat[0]) / 2;
      pSong = melody_keyboardCat;
      break;
    case 4:
      tempo = 180;
      notes = sizeof(melody_Nokia) / sizeof(melody_Nokia[0]) / 2;
      pSong = melody_Nokia;
      break;
    default:
      return false;
  }
  if(notes >= MUS_LEN - m_idx)
    notes = MUS_LEN - m_idx;

  // this calculates the duration of a whole note in ms
  int wholenote = (60000 * 4) / tempo;
  int divider = 0, noteDuration = 0;

  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2)
  {
    // calculates the duration of each note
    divider = pSong[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    add(pSong[thisNote], noteDuration * 0.9);
  }
  return true;
}

void Music::service()
{
  if(m_bPlaying == false)
    return;
  else if(millis() >= m_toneEnd)
  {
    analogWrite(TONE, 0);
    m_toneEnd = 0;
    m_bPlaying = false;
  }
  else
  {
    if(m_volume > 1)
    {
      analogWrite(TONE, m_volume);
      m_volume--;
    }
    return;
  }
  if(m_idx <= 0)
    return;
  playNote(m_arr[0].note, m_arr[0].ms);
  memcpy(m_arr, m_arr + 1, sizeof(musicArr) * MUS_LEN);
  m_idx--;
  m_bPlaying = true;
}
