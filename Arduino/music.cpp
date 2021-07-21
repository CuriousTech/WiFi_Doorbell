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

const int melody_Brahms[] = {
  // Wiegenlied (Brahms' Lullaby)
  // Score available at https://www.flutetunes.com/tunes.php?id=54

  NOTE_G4, 4, NOTE_G4, 4, //1
  NOTE_AS4, -4, NOTE_G4, 8, NOTE_G4, 4,
  NOTE_AS4, 4, REST, 4, NOTE_G4, 8, NOTE_AS4, 8,
  NOTE_DS5, 4, NOTE_D5, -4, NOTE_C5, 8,
  NOTE_C5, 4, NOTE_AS4, 4, NOTE_F4, 8, NOTE_G4, 8,
  NOTE_GS4, 4, NOTE_F4, 4, NOTE_F4, 8, NOTE_G4, 8,
  NOTE_GS4, 4, REST, 4, NOTE_F4, 8, NOTE_GS4, 8,
  NOTE_D5, 8, NOTE_C5, 8, NOTE_AS4, 4, NOTE_D5, 4,

  NOTE_DS5, 4, REST, 4, NOTE_DS4, 8, NOTE_DS4, 8, //8
  NOTE_DS5, 2, NOTE_C5, 8, NOTE_GS4, 8,
  NOTE_AS4, 2, NOTE_G4, 8, NOTE_DS4, 8,
  NOTE_GS4, 4, NOTE_AS4, 4, NOTE_C5, 4,
  NOTE_AS4, 2, NOTE_DS4, 8, NOTE_DS4, 8,
  NOTE_DS5, 2, NOTE_C5, 8, NOTE_GS4, 8,
  NOTE_AS4, 2, NOTE_G4, 8, NOTE_DS4, 8,
  NOTE_AS4, 4, NOTE_G4, 4, NOTE_DS4, 4,
  NOTE_DS4, 2
};

int melody_cantina[] = {
  // Cantina BAnd - Star wars 
  // Score available at https://musescore.com/user/6795541/scores/1606876
  NOTE_B4,-4, NOTE_E5,-4, NOTE_B4,-4, NOTE_E5,-4, 
  NOTE_B4,8,  NOTE_E5,-4, NOTE_B4,8, REST,8,  NOTE_AS4,8, NOTE_B4,8, 
  NOTE_B4,8,  NOTE_AS4,8, NOTE_B4,8, NOTE_A4,8, REST,8, NOTE_GS4,8, NOTE_A4,8, NOTE_G4,8,
  NOTE_G4,4,  NOTE_E4,-2, 
  NOTE_B4,-4, NOTE_E5,-4, NOTE_B4,-4, NOTE_E5,-4, 
  NOTE_B4,8,  NOTE_E5,-4, NOTE_B4,8, REST,8,  NOTE_AS4,8, NOTE_B4,8,

  NOTE_A4,-4, NOTE_A4,-4, NOTE_GS4,8, NOTE_A4,-4,
  NOTE_D5,8,  NOTE_C5,-4, NOTE_B4,-4, NOTE_A4,-4,
  NOTE_B4,-4, NOTE_E5,-4, NOTE_B4,-4, NOTE_E5,-4, 
  NOTE_B4,8,  NOTE_E5,-4, NOTE_B4,8, REST,8,  NOTE_AS4,8, NOTE_B4,8,
  NOTE_D5,4, NOTE_D5,-4, NOTE_B4,8, NOTE_A4,-4,
  NOTE_G4,-4, NOTE_E4,-2,
  NOTE_E4, 2, NOTE_G4,2,
  NOTE_B4, 2, NOTE_D5,2,

  NOTE_F5, -4, NOTE_E5,-4, NOTE_AS4,8, NOTE_AS4,8, NOTE_B4,4, NOTE_G4,4, 
};

const int melody_imperial[] = {
  
  // Dart Vader theme (Imperial March) - Star wars 
  // Score available at https://musescore.com/user/202909/scores/1141521
  // The tenor saxophone part was used
  
  NOTE_A4,-4, NOTE_A4,-4, NOTE_A4,16, NOTE_A4,16, NOTE_A4,16, NOTE_A4,16, NOTE_F4,8, REST,8,
  NOTE_A4,-4, NOTE_A4,-4, NOTE_A4,16, NOTE_A4,16, NOTE_A4,16, NOTE_A4,16, NOTE_F4,8, REST,8,
  NOTE_A4,4, NOTE_A4,4, NOTE_A4,4, NOTE_F4,-8, NOTE_C5,16,

  NOTE_A4,4, NOTE_F4,-8, NOTE_C5,16, NOTE_A4,2,//4
  NOTE_E5,4, NOTE_E5,4, NOTE_E5,4, NOTE_F5,-8, NOTE_C5,16,
  NOTE_A4,4, NOTE_F4,-8, NOTE_C5,16, NOTE_A4,2,
  
  NOTE_A5,4, NOTE_A4,-8, NOTE_A4,16, NOTE_A5,4, NOTE_GS5,-8, NOTE_G5,16, //7 
  NOTE_DS5,16, NOTE_D5,16, NOTE_DS5,8, REST,8, NOTE_A4,8, NOTE_DS5,4, NOTE_D5,-8, NOTE_CS5,16,

  NOTE_C5,16, NOTE_B4,16, NOTE_C5,16, REST,8, NOTE_F4,8, NOTE_GS4,4, NOTE_F4,-8, NOTE_A4,-16,//9
  NOTE_C5,4, NOTE_A4,-8, NOTE_C5,16, NOTE_E5,2,

  NOTE_A5,4, NOTE_A4,-8, NOTE_A4,16, NOTE_A5,4, NOTE_GS5,-8, NOTE_G5,16, //7 
  NOTE_DS5,16, NOTE_D5,16, NOTE_DS5,8, REST,8, NOTE_A4,8, NOTE_DS5,4, NOTE_D5,-8, NOTE_CS5,16,

  NOTE_C5,16, NOTE_B4,16, NOTE_C5,16, REST,8, NOTE_F4,8, NOTE_GS4,4, NOTE_F4,-8, NOTE_A4,-16,//9
  NOTE_A4,4, NOTE_F4,-8, NOTE_C5,16, NOTE_A4,2,
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
    analogWriteFreq(note);
    analogWrite(TONE, 40);
    m_volume = 39;
    m_bPlaying = true;
    m_toneEnd = millis() + delay;
  }
  return true;
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
      tempo = 76;
      notes = sizeof(melody_Brahms) / sizeof(melody_Brahms[0]) / 2;
      pSong = melody_Brahms;
      break;
    case 3:
      tempo = 140;
      notes = sizeof(melody_cantina) / sizeof(melody_cantina[0]) / 2;
      pSong = melody_cantina;
      break;
    case 4:
      tempo = 120;
      notes = sizeof(melody_imperial) / sizeof(melody_imperial[0]) / 2;
      pSong = melody_imperial;
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
  if(m_idx == 0)
    return;
  analogWriteFreq(m_arr[0].note);
  analogWrite(TONE, 40);
  m_volume = 39;
  m_toneEnd = millis() + m_arr[0].ms;
  memcpy(m_arr, m_arr + 1, sizeof(musicArr) * MUS_LEN);
  m_idx--;
  m_bPlaying = true;
}
