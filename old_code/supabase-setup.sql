-- ============ SUPABASE SETUP SQL ============
-- Run ini di SQL Editor Supabase https://bivdzhwizmgyvobypzys.supabase.co

-- 1. Create CALIBRATIONS table
CREATE TABLE public.calibrations (
  id bigserial PRIMARY KEY,
  nominal INTEGER NOT NULL,
  ref_r INTEGER NOT NULL,
  ref_g INTEGER NOT NULL,
  ref_b INTEGER NOT NULL,
  tolerance FLOAT NOT NULL,
  created_at TIMESTAMP WITH TIME ZONE DEFAULT now(),
  UNIQUE(nominal)
);

-- 2. Create TRANSACTIONS table
CREATE TABLE public.transactions (
  id bigserial PRIMARY KEY,
  nominal INTEGER NOT NULL,
  total_balance INTEGER NOT NULL,
  detected_at TIMESTAMP WITH TIME ZONE DEFAULT now()
);

-- 3. Enable RLS (Row Level Security) - OPTIONAL but recommended
ALTER TABLE public.calibrations ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.transactions ENABLE ROW LEVEL SECURITY;

-- 4. Create policy untuk public read (untuk API key public)
CREATE POLICY "Allow public read" ON public.calibrations
  FOR SELECT USING (true);

CREATE POLICY "Allow public read" ON public.transactions
  FOR SELECT USING (true);

CREATE POLICY "Allow public insert" ON public.calibrations
  FOR INSERT WITH CHECK (true);

CREATE POLICY "Allow public insert" ON public.transactions
  FOR INSERT WITH CHECK (true);

-- Done! Sekarang upload dan test kode Anda
